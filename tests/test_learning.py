import subprocess,sys,tempfile
from pathlib import Path
with tempfile.TemporaryDirectory() as d:
    for alg in ('ppo','alphazero-lite'):
        out=Path(d)/(alg+'.json');ck=Path(d)/(alg+'.npz')
        subprocess.run([sys.executable,'tools/learning.py',alg,'--updates','2','--envs','4','--rollout','4','--output',str(out),'--checkpoint',str(ck)],check=True)
        assert out.exists() and ck.exists()
print('learning smoke tests passed')
