import React from 'react';

interface HeaderProps {
  temp?: number;
  rpm?: number;
  isOnline: boolean;
}

export const Header: React.FC<HeaderProps> = ({ temp, rpm, isOnline }) => {
  return (
    <header className="sticky top-0 z-50 bg-bg/85 backdrop-blur-xl border-b border-white/10 px-5 py-4 flex items-center justify-between">
      <div className="flex items-center gap-2 text-lg font-black tracking-tight italic">
        <span className="text-white font-orbitron">FAN</span>
        <span className="text-accent font-orbitron">CTRL</span>
      </div>
      
      <div className="flex items-center gap-4">
        <div className="flex flex-col items-center">
           <span className="text-[9px] font-bold text-gray-500 uppercase tracking-wider">CPU</span>
           <span className="text-sm font-black text-warm leading-none">{temp !== undefined ? temp.toFixed(0) : '–'}°C</span>
        </div>
        <div className="flex flex-col items-center">
           <span className="text-[9px] font-bold text-gray-500 uppercase tracking-wider">FAN</span>
           <span className="text-sm font-black text-accent leading-none">{rpm || '–'}</span>
        </div>
        <div className={`flex items-center gap-1.5 px-3 py-1 rounded-full border text-[9px] font-bold ${isOnline ? 'bg-green/10 border-green/30 text-green' : 'bg-red/10 border-red/30 text-red'}`}>
          <span className={`w-1.5 h-1.5 rounded-full ${isOnline ? 'bg-green animate-pulse' : 'bg-red'} shadow-sm`} />
          {isOnline ? 'ONLINE' : 'OFFLINE'}
        </div>
      </div>
    </header>
  );
};
