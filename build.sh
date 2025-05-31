cd /home/emilio/git/freetribe/dsp/src/modules/wave2/util
./wav_to_array_script.sh thewave.wav thewave.data
cd /home/emilio/git/freetribe
sudo docker-compose exec freetribe make APP=waves2 MODULE=wave2
python ../hacktribe/scripts/execute_freetribe.py cpu/build/cpu.bin