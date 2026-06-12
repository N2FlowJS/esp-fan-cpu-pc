import React from 'react';
import { SystemStatus } from '../../types';

interface DashboardTabProps {
  status: SystemStatus | null;
  setMode: (m: 'manual' | 'auto') => void;
  setSpeed: (s: number) => void;
  stepSpeed: (d: number) => void;
}

export const DashboardTab: React.FC<DashboardTabProps> = ({ status, setMode, setSpeed, stepSpeed }) => {
  const speed = status?.speed || 0;
  
  return (
    <div className="space-y-4 animate-fade-up">
      {/* RPM & Speed Ring & Temp */}
      <div className="flex justify-between items-center px-6 py-4">
        <div className="flex flex-col items-center">
          <span className="text-2xl font-black tracking-tight leading-none mb-1 text-accent">{status?.rpm || '–'}</span>
          <span className="text-[10px] font-bold tracking-widest text-gray-500 uppercase">RPM</span>
        </div>
        
        <div className="relative w-36 h-32 flex items-center justify-center">
          <svg className="w-32 h-32 -rotate-90">
             <circle cx="64" cy="64" r="54" className="fill-none stroke-white/[0.08] stroke-[10]" />
             <circle 
              cx="64" cy="64" r="54" 
              className={`fill-none stroke-[10] stroke-linecap-round transition-all duration-1000 ${speed > 0 ? 'animate-pulse' : ''}`} 
              style={{ 
                strokeDasharray: 339, 
                strokeDashoffset: 339 - (339 * speed) / 100,
                stroke: `hsl(${120 - speed * 1.2}, 90%, 55%)`,
                filter: 'drop-shadow(0 0 10px currentColor)'
              }}
             />
          </svg>
          <div className="absolute inset-0 flex flex-col items-center justify-center">
             <span className="text-3xl font-black">{speed}%</span>
             <span className="text-[9px] font-extrabold text-gray-500 uppercase tracking-[0.2em]">SPEED</span>
          </div>
        </div>

        <div className="flex flex-col items-center">
          <span className="text-2xl font-black tracking-tight leading-none mb-1 text-warm">{status?.temp.toFixed(1) || '–'}</span>
          <span className="text-[10px] font-bold tracking-widest text-gray-500 uppercase">TEMP</span>
        </div>
      </div>

      {/* Mode Switcher */}
      <section className="section-card">
        <div className="flex gap-2 p-1 bg-black/20 rounded-2xl border border-white/5">
          <button 
            onClick={() => setMode('manual')}
            className={`btn-mode py-2.5 ${status?.mode === 'manual' ? 'btn-mode-active' : ''}`}
          >
            🖐 MANUAL
          </button>
          <button 
            onClick={() => setMode('auto')}
            className={`btn-mode py-2.5 ${status?.mode === 'auto' ? 'btn-mode-active' : ''}`}
          >
            🤖 AUTO
          </button>
        </div>
      </section>

      {/* Manual Controls */}
      <section className={`section-card transition-all duration-500 ${status?.mode === 'auto' ? 'opacity-30 grayscale pointer-events-none' : ''}`}>
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-accent rounded-full shadow-[0_0_10px_rgba(99,179,237,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-accent uppercase">CONTROL PANEL</h2>
        </div>

        <div className="grid grid-cols-6 gap-2 mb-6">
          {[-10, -5, -1, 1, 5, 10].map(v => (
            <button 
              key={v}
              onClick={() => stepSpeed(v)}
              className={`py-3.5 rounded-xl border text-xs font-black transition-all active:scale-90 ${
                v < 0 ? 'bg-red/5 border-red/20 text-red hover:bg-red/10' : 'bg-green/5 border-green/20 text-green hover:bg-green/10'
              }`}
            >
              {v > 0 ? `+${v}` : v}
            </button>
          ))}
        </div>

        <div className="px-1 mb-8">
          <input 
            type="range" min="0" max="100" 
            value={speed} 
            onChange={(e) => setSpeed(parseInt(e.target.value))}
            className="w-full h-1.5 bg-white/10 rounded-full appearance-none cursor-pointer accent-accent shadow-inner"
          />
          <div className="flex justify-between mt-3 px-1 text-[9px] font-black text-gray-600 tracking-tighter">
            <span>0% (OFF)</span>
            <span>25</span>
            <span>50</span>
            <span>75</span>
            <span>100% (MAX)</span>
          </div>
        </div>

        <div className="flex flex-wrap gap-2">
          {[0, 25, 50, 75, 100].map(p => (
            <button 
              key={p} onClick={() => setSpeed(p)}
              className={`flex-1 min-w-[50px] py-2.5 rounded-xl border font-black text-[10px] transition-all active:scale-95 ${
                speed === p 
                  ? 'bg-accent border-accent text-bg shadow-lg shadow-accent/20' 
                  : 'bg-white/[0.03] border-white/10 text-gray-500 hover:border-accent/40 hover:text-gray-300'
              }`}
            >
              {p === 0 ? 'OFF' : p === 100 ? 'MAX' : `${p}%`}
            </button>
          ))}
        </div>
      </section>
    </div>
  );
};
