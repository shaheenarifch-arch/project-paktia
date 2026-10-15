PROJECT PAKTIA 3D AVATAR OVERLAY

1) Copy avatar3d.ts to ui/src/avatar3d.ts
2) Copy avatar.css to ui/src/avatar.css
3) cd ui && npm install three && npm install -D @types/three
4) In ui/src/main.ts import both files and mount after rendering:
   import { mountHologramAvatar } from './avatar3d'
   import './avatar.css'
5) Put this where the circular avatar belongs:
   <div class="avatar-orbit"><span class="avatar-dot"></span><div class="avatar-window"><canvas id="commandAvatar3d"></canvas></div></div>
6) After innerHTML: const avatarCanvas=document.querySelector<HTMLCanvasElement>('#commandAvatar3d'); if(avatarCanvas) mountHologramAvatar(avatarCanvas)
7) npm run build && npm run dev -- --host 0.0.0.0
