@echo off
set "EXE=%~dp0..\build-codex-vs\src\Release\bomber_viz.exe"
if not exist "%EXE%" echo Build Release first: cmake --build build-codex-vs --config Release & pause & exit /b 1
start "AI Bomber MCTS" "%EXE%" --agent mcts --enemy heuristic --agents 2 --fps 60 --start-paused
