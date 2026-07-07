"""Reproducible NumPy AlphaZero-lite and PPO baselines for AI Bomber.

The compact training arena uses the project's six actions and board-channel
encoding. It is intentionally framework-free and is a comparison baseline,
not a claim of full AlphaZero or superhuman play.
"""
from __future__ import annotations
import argparse, json, math, os, platform, subprocess, time
from pathlib import Path
import numpy as np

ACTIONS=6
class VecArena:
    def __init__(self,n=16,seed=1,size=7,max_steps=40): self.n=n;self.size=size;self.max_steps=max_steps;self.rng=np.random.default_rng(seed);self.reset()
    def reset(self):
        self.pos=np.ones((self.n,2),dtype=np.int32);self.goal=np.full((self.n,2),self.size-2,dtype=np.int32);self.steps=np.zeros(self.n,dtype=np.int32);return self.obs()
    def obs(self):
        x=np.zeros((self.n,4,self.size,self.size),dtype=np.float32);x[:,0,0,:]=x[:,0,-1,:]=1;x[:,0,:,0]=x[:,0,:,-1]=1
        for i in range(self.n):x[i,1,self.pos[i,1],self.pos[i,0]]=1;x[i,2,self.goal[i,1],self.goal[i,0]]=1
        x[:,3]=self.steps[:,None,None]/self.max_steps;return x.reshape(self.n,-1)
    def step(self,a):
        delta=np.array([[0,-1],[0,1],[-1,0],[1,0],[0,0],[0,0]],dtype=np.int32);old=self.pos.copy();self.pos+=delta[a];self.pos=np.clip(self.pos,1,self.size-2);self.steps+=1
        dist=np.abs(self.pos-self.goal).sum(1);old_dist=np.abs(old-self.goal).sum(1);win=dist==0;done=win|(self.steps>=self.max_steps);r=(old_dist-dist)*.1+win*1.0-.01
        if np.any(done):self.pos[done]=1;self.steps[done]=0
        return self.obs(),r.astype(np.float32),done,win

class Net:
    def __init__(self,dim,seed=1):
        r=np.random.default_rng(seed);self.w=r.normal(0,.02,(dim,ACTIONS));self.v=r.normal(0,.02,dim)
    def policy(self,x):z=x@self.w;z-=z.max(1,keepdims=True);p=np.exp(z);return p/p.sum(1,keepdims=True)
    def value(self,x):return np.tanh(x@self.v)
    def save(self,path,meta):Path(path).parent.mkdir(parents=True,exist_ok=True);np.savez(path,w=self.w,v=self.v,meta=json.dumps(meta))
    @classmethod
    def load(cls,path):d=np.load(path);n=cls(d['w'].shape[0]);n.w=d['w'];n.v=d['v'];return n

def evaluate(net,seeds=(9001,9002,9003),episodes=24):
    wins=steps=0
    for s in seeds:
        e=VecArena(episodes,s);o=e.obs();finished=np.zeros(episodes,bool);rng=np.random.default_rng(s)
        for _ in range(e.max_steps):
            a=net.policy(o).argmax(1) if net is not None else rng.integers(0,ACTIONS,episodes)
            o,_,d,w=e.step(a);wins+=int((w&~finished).sum());finished|=d;steps+=int((~finished).sum())
            if finished.all():break
    return {'episodes':episodes*len(seeds),'win_rate':wins/(episodes*len(seeds)),'steps':steps}

def ppo(args):
    env=VecArena(args.envs,args.seed);net=Net(env.obs().shape[1],args.seed);rng=np.random.default_rng(args.seed);logs=[];start=time.time()
    for update in range(args.updates):
        xs=[];acts=[];rews=[];vals=[];o=env.obs()
        for _ in range(args.rollout):
            p=net.policy(o);a=np.array([rng.choice(ACTIONS,p=q) for q in p]);v=net.value(o);n,r,_,_=env.step(a);xs.append(o);acts.append(a);rews.append(r);vals.append(v);o=n
        x=np.concatenate(xs);a=np.concatenate(acts);r=np.concatenate(rews);v=np.concatenate(vals);adv=r-v;old=net.policy(x)[np.arange(len(a)),a]
        for _ in range(4):
            p=net.policy(x);chosen=p[np.arange(len(a)),a];ratio=chosen/(old+1e-8);clipped=np.clip(ratio,1-args.clip,1+args.clip);weight=np.where(adv>=0,np.minimum(ratio,clipped),np.maximum(ratio,clipped))*adv
            one=np.eye(ACTIONS)[a];net.w+=args.lr*x.T@((one-p)*weight[:,None])/len(x);net.v+=args.lr*x.T@(r-net.value(x))/len(x)
        logs.append({'update':update,'return':float(r.mean()),'entropy':float(-(p*np.log(p+1e-8)).sum(1).mean()),'value_loss':float(((r-net.value(x))**2).mean())})
    return finish('ppo',net,args,logs,start)

def alphazero(args):
    env=VecArena(args.envs,args.seed);net=Net(env.obs().shape[1],args.seed);rng=np.random.default_rng(args.seed);logs=[];start=time.time();o=env.obs()
    for update in range(args.updates):
        p=net.policy(o);targets=np.full_like(p,.01)
        # Lightweight search targets: reward one-step moves that approach the goal.
        for act in range(4):
            delta=np.array([[0,-1],[0,1],[-1,0],[1,0]])[act];new=np.clip(env.pos+delta,1,env.size-2);targets[:,act]=np.exp((np.abs(env.pos-env.goal).sum(1)-np.abs(new-env.goal).sum(1))*2.0)
        targets/=targets.sum(1,keepdims=True);a=np.array([rng.choice(ACTIONS,p=q) for q in targets]);n,r,_,_=env.step(a);pred=net.policy(o);net.w+=args.lr*o.T@(targets-pred)/len(o);net.v+=args.lr*o.T@(r-net.value(o))/len(o);o=n
        logs.append({'update':update,'policy_loss':float(-(targets*np.log(pred+1e-8)).sum(1).mean()),'value_loss':float(((r-net.value(o))**2).mean()),'entropy':float(-(pred*np.log(pred+1e-8)).sum(1).mean())})
    return finish('alphazero-lite',net,args,logs,start)

def finish(name,net,args,logs,start):
    ev=evaluate(net);baseline=evaluate(None)
    try: sha=subprocess.check_output(['git','rev-parse','--short=12','HEAD'],text=True).strip()
    except Exception: sha='unknown'
    meta={'algorithm':name,'seed':args.seed,'git_sha':sha,'hardware':platform.platform(),'config':vars(args),'seed_suite':'holdout: 9001,9002,9003','holdout':ev,'random_holdout':baseline,'above_random':ev['win_rate']>baseline['win_rate'],'reward_shaping_limitations':'Dense distance reward simplifies exploration and may not transfer to the full C arena.','steps_per_sec':args.envs*args.rollout*max(args.updates,1)/max(time.time()-start,1e-6)}
    net.save(args.checkpoint,meta);Path(args.output).parent.mkdir(parents=True,exist_ok=True);Path(args.output).write_text(json.dumps({'schema_version':1,'metadata':meta,'learning_curve':logs},indent=2));print(json.dumps(meta,indent=2));return meta

def main():
    p=argparse.ArgumentParser();p.add_argument('algorithm',choices=['ppo','alphazero-lite']);p.add_argument('--seed',type=int,default=1);p.add_argument('--updates',type=int,default=30);p.add_argument('--envs',type=int,default=16);p.add_argument('--rollout',type=int,default=16);p.add_argument('--lr',type=float,default=.03);p.add_argument('--clip',type=float,default=.2);p.add_argument('--checkpoint',default='results/checkpoint.npz');p.add_argument('--output',default='results/learning.json');a=p.parse_args();(ppo if a.algorithm=='ppo' else alphazero)(a)
if __name__=='__main__':main()
