import React, { useState, useEffect, useRef } from 'react';
import { SystemStatus } from '../../types';
import { AlertTriangle, Cpu, Monitor } from 'lucide-react';

interface DashboardTabProps {
  status: SystemStatus | null;
  setMode: (m: 'manual' | 'auto') => void;
  setSpeed: (s: number) => void;
  stepSpeed: (d: number) => void;
  setLedConfig: (mode: string, color?: string, brightness?: number) => void;
}

export const DashboardTab: React.FC<DashboardTabProps> = ({ status, setMode, setSpeed, stepSpeed, setLedConfig }) => {
  const speed = status?.speed || 0;
  
  // Local state for smooth dragging without network lag
  const [localSpeed, setLocalSpeed] = useState<number | null>(null);
  const [localBrightness, setLocalBrightness] = useState<number | null>(null);
  const [localColor, setLocalColor] = useState<string | null>(null);
  const debounceTimerRef = useRef<NodeJS.Timeout | null>(null);
  const brightnessDebounceRef = useRef<NodeJS.Timeout | null>(null);
  const colorDebounceRef = useRef<NodeJS.Timeout | null>(null);

  // Sync local state with backend status once it matches
  useEffect(() => {
    if (localSpeed !== null && status?.speed === localSpeed) {
      setLocalSpeed(null);
    }
  }, [status?.speed, localSpeed]);

  useEffect(() => {
    if (localBrightness !== null && status?.ledBrightness === localBrightness) {
      setLocalBrightness(null);
    }
  }, [status?.ledBrightness, localBrightness]);

  useEffect(() => {
    if (localColor !== null && status?.ledColor?.toLowerCase() === localColor.toLowerCase()) {
      setLocalColor(null);
    }
  }, [status?.ledColor, localColor]);

  // Clean up debounce timers on unmount
  useEffect(() => {
    return () => {
      if (debounceTimerRef.current) clearTimeout(debounceTimerRef.current);
      if (brightnessDebounceRef.current) clearTimeout(brightnessDebounceRef.current);
      if (colorDebounceRef.current) clearTimeout(colorDebounceRef.current);
    };
  }, []);

  const handleSliderChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = parseInt(e.target.value);
    setLocalSpeed(val);
    if (debounceTimerRef.current) clearTimeout(debounceTimerRef.current);
    debounceTimerRef.current = setTimeout(() => {
      setSpeed(val);
    }, 150);
  };

  const handleBrightnessChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = parseInt(e.target.value);
    setLocalBrightness(val);
    if (brightnessDebounceRef.current) clearTimeout(brightnessDebounceRef.current);
    brightnessDebounceRef.current = setTimeout(() => {
      setLedConfig(status?.ledMode || 'auto', status?.ledColor, val);
    }, 250);
  };

  const handleColorChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = e.target.value;
    setLocalColor(val);
    if (colorDebounceRef.current) clearTimeout(colorDebounceRef.current);
    colorDebounceRef.current = setTimeout(() => {
      setLedConfig('manual', val, currentDisplayBrightness);
    }, 250);
  };

  const handleSliderRelease = () => {
    if (localSpeed !== null) {
      if (debounceTimerRef.current) {
        clearTimeout(debounceTimerRef.current);
        debounceTimerRef.current = null;
      }
      setSpeed(localSpeed);
    }
  };

  const handleBrightnessRelease = () => {
    if (localBrightness !== null) {
      if (brightnessDebounceRef.current) {
        clearTimeout(brightnessDebounceRef.current);
        brightnessDebounceRef.current = null;
      }
      setLedConfig(status?.ledMode || 'auto', status?.ledColor, localBrightness);
    }
  };

  const currentDisplaySpeed = localSpeed !== null ? localSpeed : speed;
  const currentDisplayBrightness = localBrightness !== null ? localBrightness : (status?.ledBrightness ?? 128);
  const currentDisplayColor = localColor !== null ? localColor : (status?.ledColor || '#000000');
  
  return (
    <div className="space-y-6 animate-fade-up">
      {/* Health Alert Banner */}
      {status?.healthAlert && (
        <div className="bg-red/10 border border-red/30 rounded-2xl p-4 flex items-center gap-4 animate-pulse">
           <div className="p-3 bg-red/20 rounded-xl">
             <AlertTriangle size={24} className="text-red" />
           </div>
           <div className="flex-1">
             <h3 className="text-sm font-black text-red tracking-tight uppercase">FAN STALL DETECTED</h3>
             <p className="text-[10px] font-bold text-red/70 uppercase tracking-tighter leading-none mt-1">PWM is active but RPM is zero. Check for obstructions or hardware failure.</p>
           </div>
        </div>
      )}

      {/* RPM & Speed Ring & Temp */}
      <div className="flex flex-col sm:flex-row justify-between items-center gap-6 px-4 md:px-8 py-6 bg-white/[0.01] rounded-3xl border border-white/5 shadow-inner">
        <div className="flex flex-col items-center order-2 sm:order-1">
          <span className="text-3xl md:text-4xl font-black tracking-tighter leading-none mb-1 text-accent">{status?.rpm || '–'}</span>
          <span className="text-[10px] md:text-xs font-black tracking-[0.2em] text-gray-600 uppercase">RPM</span>
        </div>
        
        <div className="relative w-40 h-40 flex items-center justify-center order-1 sm:order-2">
          <svg className="w-40 h-40 -rotate-90">
             <circle cx="80" cy="80" r="70" className="fill-none stroke-white/[0.08] stroke-[12]" />
             <circle 
              cx="80" cy="80" r="70" 
              className={`fill-none stroke-[12] stroke-linecap-round transition-all duration-1000 ${currentDisplaySpeed > 0 ? 'animate-pulse' : ''}`} 
              style={{ 
                strokeDasharray: 440, 
                strokeDashoffset: 440 - (440 * currentDisplaySpeed) / 100,
                stroke: `hsl(${120 - currentDisplaySpeed * 1.2}, 90%, 55%)`,
                filter: 'drop-shadow(0 0 15px currentColor)'
              }}
             />
          </svg>
          <div className="absolute inset-0 flex flex-col items-center justify-center">
             <span className="text-4xl font-black tracking-tighter">{currentDisplaySpeed}%</span>
             <span className="text-[10px] font-black text-gray-500 uppercase tracking-[0.3em]">SPEED</span>
          </div>
        </div>

        <div className="flex flex-col items-center order-3">
          <div className="flex items-center gap-2 mb-1">
            <span className="text-3xl md:text-4xl font-black tracking-tighter leading-none text-warm">{status?.temp.toFixed(1) || '–'}</span>
            <div className={`p-1 rounded-md border ${status?.hostTempActive ? 'bg-accent/10 border-accent/30 text-accent' : 'bg-white/5 border-white/10 text-gray-600'}`} title={status?.hostTempActive ? "Source: PC Host" : "Source: ESP32 Internal"}>
               {status?.hostTempActive ? <Monitor size={12} /> : <Cpu size={12} />}
            </div>
          </div>
          <span className="text-[10px] md:text-xs font-black tracking-[0.2em] text-gray-600 uppercase">TEMP</span>
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
            value={currentDisplaySpeed} 
            onChange={handleSliderChange}
            onMouseUp={handleSliderRelease}
            onTouchEnd={handleSliderRelease}
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
                currentDisplaySpeed === p 
                  ? 'bg-accent border-accent text-bg shadow-lg shadow-accent/20' 
                  : 'bg-white/[0.03] border-white/10 text-gray-500 hover:border-accent/40 hover:text-gray-300'
              }`}
            >
              {p === 0 ? 'OFF' : p === 100 ? 'MAX' : `${p}%`}
            </button>
          ))}
        </div>
      </section>

      {/* LED Control Panel */}
      <section className="section-card mt-6">
        <div className="flex items-center gap-2 mb-5">
          <div className="w-1.5 h-3.5 bg-purple-500 rounded-full shadow-[0_0_10px_rgba(168,85,247,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-purple-400 uppercase">LED RGB CONTROL</h2>
        </div>

        <div className="flex gap-2 p-1 bg-black/20 rounded-2xl border border-white/5 mb-6">
          <button 
            onClick={() => setLedConfig('auto', undefined, currentDisplayBrightness)}
            className={`flex-1 py-2 rounded-xl text-[10px] font-black tracking-widest transition-all ${status?.ledMode !== 'manual' ? 'bg-purple-500 text-white shadow-[0_0_15px_rgba(168,85,247,0.4)]' : 'text-gray-500 hover:text-white'}`}
          >
            AUTO SYNC
          </button>
          <button 
            onClick={() => setLedConfig('manual', status?.ledColor || '#00FF00', currentDisplayBrightness)}
            className={`flex-1 py-2 rounded-xl text-[10px] font-black tracking-widest transition-all ${status?.ledMode === 'manual' ? 'bg-purple-500 text-white shadow-[0_0_15px_rgba(168,85,247,0.4)]' : 'text-gray-500 hover:text-white'}`}
          >
            MANUAL
          </button>
        </div>

        <div className="px-1 mb-8">
          <div className="flex justify-between items-center mb-3">
            <span className="text-[10px] font-extrabold text-gray-500 tracking-widest uppercase">BRIGHTNESS</span>
            <span className="text-xs font-black text-purple-400">{Math.round((currentDisplayBrightness / 255) * 100)}%</span>
          </div>
          <input 
            type="range" min="0" max="255" 
            value={currentDisplayBrightness} 
            onChange={handleBrightnessChange}
            onMouseUp={handleBrightnessRelease}
            onTouchEnd={handleBrightnessRelease}
            className="w-full h-1.5 bg-white/10 rounded-full appearance-none cursor-pointer accent-purple-500 shadow-inner"
          />
        </div>

        <div className={`transition-all duration-300 ${status?.ledMode !== 'manual' ? 'opacity-30 pointer-events-none grayscale' : ''}`}>
          <div className="flex items-center gap-4">
            <div className="relative w-14 h-14 rounded-xl overflow-hidden border-2 border-white/10 shrink-0 shadow-lg">
              <input 
                type="color" 
                value={currentDisplayColor}
                onChange={handleColorChange}
                className="absolute -inset-2 w-20 h-20 cursor-pointer"
              />
            </div>
            <div className="flex-1">
              <span className="text-[9px] font-black text-gray-500 uppercase tracking-widest block mb-1">SELECTED COLOR</span>
              <span className="font-mono text-sm tracking-wider font-bold text-white/90">{currentDisplayColor.toUpperCase()}</span>
            </div>
            <button 
              onClick={() => {
                setLocalColor('#000000');
                setLedConfig('manual', '#000000', currentDisplayBrightness);
              }}
              className="px-5 py-3 bg-red-500/10 text-red-500 border border-red-500/20 rounded-xl text-[10px] font-black tracking-widest uppercase hover:bg-red-500/20 active:scale-95 transition-all shadow-lg"
            >
              TURN OFF
            </button>
          </div>
        </div>
      </section>
    </div>
  );
};
