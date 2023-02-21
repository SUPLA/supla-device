#!/bin/bash
exec 2> /dev/null

# Put your Android TV IP address
android_tv_ip="192.168.8.8"

trap cleanup INT

cleanup() {
  echo "Exiting the script due to user interruption."
  exit 0
}

log_to_file() {
  logfile="/dev/null"
# To enable logs, uncomment the following line and comment the line above.
  #logfile="/home/supla_user/android_tv/atv.log"
  echo "$1" | ts "%H:%M:%.S" >> $logfile
}

write_number_if_different () {
    number=$1
    # Adjust atv.state file name and location. Use absolute path without ~
    filename="/home/supla_user/android_tv/atv.state"
    current_value=`cat $filename`

    if [[ $current_value != $number ]]; then
      log_to_file "value different ${number} file: ${current_value}"
      echo "$number" > "$filename"
    fi
    touch "$filename"
}


log_to_file "Starting the script"

while true; do

  dumpsys_result=$(timeout 3 adb shell dumpsys media_session)

  if [ "$?" != 0 ]
  then
    write_number_if_different "-1"
    adb disconnect $android_tv_ip > /dev/null
    timeout 5 adb connect $android_tv_ip > /dev/null
    if [ "$?" != 0 ]
    then
      log_to_file "failed to connect"
      write_number_if_different "-1"
      continue
    fi

    devices_lines=$(adb devices | wc -l)
    if [ "$devices_lines" -lt 3 ]
    then
      log_to_file "adb devices failed"
      write_number_if_different "-1"
      continue
    fi

    log_to_file "dumpsys m_s failed"
    continue
  fi

  log_to_file "==========================="
  log_to_file "$dumpsys_result"
  log_to_file "==========================="

  result=$(echo "$dumpsys_result" | grep -E "Media button session is.*userId=0" -A11 | grep -oE "state=[0-9]+" | grep -o "[0-9]*")

  if [ "$result" = "" ]
  then
    log_to_file "adb m_s was empty, checking audio"
    audio_full_result=$(adb shell dumpsys audio)
    log_to_file "$audio_full_result"
    log_to_file "==========================="

    audio_result=$(echo "$audio_full_result" | awk 'index($0,"players:"){p=1}p{if(/^$/){exit};print}' | grep -E "usage=(USAGE_MEDIA|USAGE_GAME)" | grep -o "state:[a-z]*") 

    if [ "$audio_full_result" = "" ]
      then
        write_number_if_different "-1"
        sleep 1
        continue
    elif [[ "$audio_result" == *"started"* ]]
    then
      log_to_file "audio started"
      write_number_if_different "3"
    elif [[ "$audio_result" == *"paused"* ]]
    then
      log_to_file "audio paused"
      write_number_if_different "2"
    elif [[ "$audio_result" == *"stopped"* ]]
    then
      log_to_file "audio stopped"
      write_number_if_different "1"
    else
      log_to_file "audio unknown"
      write_number_if_different "0"
    fi

    sleep 1
    continue
  fi

  log_to_file "result ${result}"
  write_number_if_different $result
  sleep 1
  continue
done
