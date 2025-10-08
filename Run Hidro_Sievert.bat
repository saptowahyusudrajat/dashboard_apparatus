@echo off
echo Starting Node-RED...
start cmd /k node-red

echo Waiting for Node-RED to start...
timeout /t 5 /nobreak >nul

echo Opening Node-RED Dashboard in browser...
start http://localhost:1880/ui

pause
