cd /home/io/projects/learn_erl/otp/erts/example/mini_beam_esp32/zephyr_app

# verify only one target board is connected
ls -l /dev/ttyACM*

# manual capture (known-good path)
mkdir -p logs
stdbuf -oL -eL picocom -b 115200 /dev/ttyACM0 --imap lfcrlf | stdbuf -oL -eL tee logs/golden_10m_ttyACM0.log

#
./analyze_event_perf.sh logs/golden_10m_ttyACM0.log --scenario golden_10m_ttyACM0 --csv logs/golden_10m_ttyACM0.csv --json logs/golden_10m_ttyACM0.json
#
./check_perf_regression.sh logs/golden_10m_ttyACM0.csv --scenario golden_10m_ttyACM0 --min-event-rate-hz 2.0 --max-drop-pct 0.10 --min-processed-pct 99.0 --max-sensor-p99-ms 1300


./erts/example/mini_beam_esp32/zephyr_app/golden_single_mast.sh
