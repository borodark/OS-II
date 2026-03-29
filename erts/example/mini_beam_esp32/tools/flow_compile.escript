#!/usr/bin/env escript
%% OS/II Flow Compiler (P2)
%%
%% Reads a .flow file (Erlang term) and emits a C header with bytecode
%% arrays for the sensor process and actuator process.
%%
%% Usage: flow_compile.escript <input.flow> <output.h>

-mode(compile).

-define(OP_CONST_I32,  16#01).
-define(OP_CALL_BIF,   16#10).
-define(OP_RECV_CMD,   16#20).
-define(OP_SEND,       16#21).
-define(OP_SLEEP_MS,   16#40).
-define(OP_JMP,        16#30).

-define(BIF_PWM_SET_DUTY, 2).
-define(BIF_I2C_READ_REG, 3).
-define(BIF_MONOTONIC_MS, 4).

-define(CMD_PWM_SET_DUTY, 2).

main([InFile, OutFile]) ->
    case file:consult(InFile) of
        {ok, [Flow]} ->
            validate(Flow),
            {SensorProg, ActuatorProg} = compile_flow(Flow),
            Header = emit_header(Flow, SensorProg, ActuatorProg),
            ok = file:write_file(OutFile, Header),
            io:format("flow_compile: ~s -> ~s~n", [InFile, OutFile]),
            io:format("  sensor:   ~p bytes~n", [length(SensorProg)]),
            io:format("  actuator: ~p bytes~n", [length(ActuatorProg)]),
            io:format("  flows:    ~p~n", [length(maps:get(flows, Flow))]);
        {error, Reason} ->
            io:format(standard_error, "error: ~s: ~p~n", [InFile, Reason]),
            halt(1)
    end;
main(_) ->
    io:format(standard_error, "usage: flow_compile.escript <input.flow> <output.h>~n", []),
    halt(1).

%% --- validation ---

validate(#{sensors := Ss, actuators := As, flows := Fs, policy := P}) ->
    lists:foreach(fun validate_sensor/1, Ss),
    lists:foreach(fun validate_actuator/1, As),
    lists:foreach(fun(F) -> validate_flow(F, Ss, As) end, Fs),
    validate_policy(P);
validate(_) ->
    fail("flow must be a map with keys: sensors, actuators, flows, policy").

validate_sensor(#{bus := B, addr := A, reg := R, poll_ms := P})
  when is_integer(B), B >= 0, B =< 3,
       is_integer(A), A >= 0, A =< 127,
       is_integer(R), R >= 0, R =< 255,
       is_integer(P), P > 0 -> ok;
validate_sensor(S) -> fail("invalid sensor: ~p", [S]).

validate_actuator(#{kind := pwm, channel := C})
  when is_integer(C), C >= 0, C =< 7 -> ok;
validate_actuator(A) -> fail("invalid actuator: ~p", [A]).

validate_flow(#{from := Addr, to := {pwm, Ch}}, Ss, As) ->
    lists:any(fun(#{addr := A}) -> A =:= Addr end, Ss)
        orelse fail("flow: unknown sensor addr=0x~.16B", [Addr]),
    lists:any(fun(#{channel := C}) -> C =:= Ch end, As)
        orelse fail("flow: unknown actuator channel=~p", [Ch]);
validate_flow(F, _, _) -> fail("invalid flow: ~p", [F]).

validate_policy(#{mailbox_depth := M, watchdog_ms := W, on_fail := F})
  when is_integer(M), M > 0, is_integer(W), W > 0,
       (F =:= stop_actuator orelse F =:= hold_last orelse F =:= ignore) -> ok;
validate_policy(P) -> fail("invalid policy: ~p", [P]).

fail(Fmt) -> fail(Fmt, []).
fail(Fmt, Args) ->
    io:format(standard_error, "error: " ++ Fmt ++ "~n", Args), halt(1).

%% --- bytecode compilation ---

compile_flow(#{sensors := Sensors, flows := Flows}) ->
    [#{from := SAddr, to := {pwm, PwmCh}} | _] = Flows,
    #{bus := Bus, reg := Reg, poll_ms := Poll} =
        hd([S || S = #{addr := A} <- Sensors, A =:= SAddr]),
    {compile_sensor(Bus, SAddr, Reg, Poll, PwmCh),
     compile_actuator()}.

compile_sensor(Bus, Addr, Reg, PollMs, PwmCh) ->
    %% Register plan:
    %%   r0=bus r1=addr r2=reg r3=actuator_pid(2) r4=CMD_PWM r5=ch r6=poll r7=value r8=0 r9=ts
    Init = lists:flatten([
        const_i32(0, Bus),
        const_i32(1, Addr),
        const_i32(2, Reg),
        const_i32(3, 2),               % actuator pid
        const_i32(4, ?CMD_PWM_SET_DUTY),
        const_i32(5, PwmCh),
        const_i32(6, PollMs),
        const_i32(8, 0)                % zero for unused cmd fields
    ]),
    LoopPC = length(Init),
    Body = lists:flatten([
        [?OP_CALL_BIF, ?BIF_I2C_READ_REG, 3, 0, 1, 2, 7],  % r7 = i2c_read(r0,r1,r2)
        [?OP_CALL_BIF, ?BIF_MONOTONIC_MS, 0, 9],             % r9 = now()
        [?OP_SEND, 3, 4, 5, 7, 8, 8],                        % send to actuator
        [?OP_SLEEP_MS, 6]                                     % sleep poll_ms
    ]),
    JmpOff = LoopPC - (LoopPC + length(Body) + 5),           % +5 = opcode + i32
    Init ++ Body ++ [?OP_JMP | i32le(JmpOff)].

compile_actuator() ->
    %% r0=type r1=channel r2=duty r3=c r4=d
    Body = lists:flatten([
        [?OP_RECV_CMD, 0, 1, 2, 3, 4],                       % recv (blocks)
        [?OP_CALL_BIF, ?BIF_PWM_SET_DUTY, 2, 1, 2, 5]        % pwm_set_duty(r1,r2)->r5
    ]),
    JmpOff = 0 - (length(Body) + 5),
    Body ++ [?OP_JMP | i32le(JmpOff)].

%% --- helpers ---

const_i32(R, V) -> [?OP_CONST_I32, R | i32le(V)].

i32le(V) when V >= 0 ->
    [V band 16#FF, (V bsr 8) band 16#FF, (V bsr 16) band 16#FF, (V bsr 24) band 16#FF];
i32le(V) -> i32le((1 bsl 32) + V).

%% --- C header output ---

emit_header(Flow, SProg, AProg) ->
    #{sensors := Ss, policy := #{mailbox_depth := MD, watchdog_ms := WD, on_fail := OF}} = Flow,
    #{bus := B, addr := A, reg := R, poll_ms := P} = hd(Ss),
    OFS = atom_to_list(OF),
    lists:flatten([
        "/* Generated by OS/II flow compiler -- do not edit */\n",
        "#ifndef OS2_FLOW_GENERATED_H\n#define OS2_FLOW_GENERATED_H\n\n",
        io_lib:format("/* Flow: sensor bus=~B addr=0x~2.16.0B reg=0x~2.16.0B poll=~Bms -> pwm */~n", [B,A,R,P]),
        io_lib:format("/* Policy: mailbox=~B watchdog=~Bms on_fail=~s */~n~n", [MD,WD,OFS]),
        io_lib:format("#define OS2_FLOW_SENSOR_COUNT ~B~n", [length(Ss)]),
        io_lib:format("#define OS2_FLOW_POLL_MS ~B~n", [P]),
        io_lib:format("#define OS2_FLOW_MAILBOX_DEPTH ~B~n", [MD]),
        io_lib:format("#define OS2_FLOW_WATCHDOG_MS ~B~n", [WD]),
        io_lib:format("#define OS2_FLOW_ON_FAIL \"~s\"~n~n", [OFS]),
        arr("os2_flow_sensor_prog", SProg), "\n",
        arr("os2_flow_actuator_prog", AProg), "\n",
        "#endif\n"
    ]).

arr(Name, Bs) ->
    [io_lib:format("static const uint8_t ~s[~B] = {\n    ", [Name, length(Bs)]),
     fmt_bytes(Bs, 0), "\n};\n"].

fmt_bytes([], _) -> [];
fmt_bytes([B], _) -> io_lib:format("0x~2.16.0B", [B]);
fmt_bytes([B|R], C) when C > 0, C rem 12 =:= 0 ->
    [io_lib:format("0x~2.16.0B,\n    ", [B]) | fmt_bytes(R, C+1)];
fmt_bytes([B|R], C) ->
    [io_lib:format("0x~2.16.0B, ", [B]) | fmt_bytes(R, C+1)].
