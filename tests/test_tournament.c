#include "sim/benchmark.h"
#include <assert.h>
#include <stdio.h>
int main(void){BomberConfig c;config_battle(&c);c.agent_count=2;c.max_steps=10;int ok=benchmark_matrix(&c,1,9001,"holdout","tournament-smoke.json",1);assert(ok);if(!ok)return 1;FILE*f=fopen("tournament-smoke.json","r");assert(f);if(!f)return 1;char b[64]={0};size_t n=fread(b,1,63,f);assert(n>0);fclose(f);assert(b[0]=='{');remove("tournament-smoke.json");puts("tournament tests passed");return 0;}
