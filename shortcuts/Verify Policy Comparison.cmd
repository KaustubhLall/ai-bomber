@echo off
set "EXE=%~dp0..\build-codex-vs\src\Release\bomber_viz.exe"
if not exist "%EXE%" echo Build Release first: cmake --build build-codex-vs --config Release & pause & exit /b 1
start "AI Bomber Matchup Board" "%EXE%" --view compare --matchup mcts:heuristic --matchup heuristic:mcts --matchup mcts:greedy --matchup greedy:mcts --matchup mcts:alpha-beta --matchup alpha-beta:mcts --epochs 20 --fps 3 --render-fps 60
