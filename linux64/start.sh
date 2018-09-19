#!/bin/bash
kill -9 `pidof ManagerSrv`
kill -9 `pidof plate`
nohup ./build/plate &
echo `pidof plate`
