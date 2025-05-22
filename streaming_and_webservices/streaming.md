# RTSP
### Download
v4l2rtspserver
### Run
```
v4l2rtspserver -W 1280 -H 720 -F 30 -P 8080 -S /dev/video0 
```
### Access method
1. Open VLC Player
2. Select Open Network Stream
3. Open rtsp://10.0.34.126:8080/unicast

# HTTP
### Download
mjpg-streamer
### Run
```
sh /home/kng/mjpg-streamer/mjpg-streamer-experimental/start.sh
```
