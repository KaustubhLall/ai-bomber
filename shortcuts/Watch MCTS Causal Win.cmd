@echo off
set "EXE=%~dp0..\build-codex-vs\src\Release\bomber_viz.exe"
set "REPLAY=%~dp0..\results\mcts-safe-greedy-win.bin"
if not exist "%EXE%" echo Build Release first: cmake --build build-codex-vs --config Release & pause & exit /b 1
if not exist "%REPLAY%" echo Replay not found: %REPLAY% & pause & exit /b 1
start "AI Bomber MCTS Causal Win" "%EXE%" --replay "%REPLAY%" --fps 3 --render-fps 60
