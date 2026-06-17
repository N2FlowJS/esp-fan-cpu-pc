import React from 'react';
import { Activity, Wifi, Settings } from 'lucide-react';

interface TabNavProps {
  activeTab: string;
  onTabChange: (tab: any) => void;
}

export const TabNav: React.FC<TabNavProps> = ({ activeTab, onTabChange }) => {
  const tabs = [
    { id: 'dash', label: 'DASHBOARD', icon: Activity },
    { id: 'snif', label: 'SNIFFER', icon: Wifi },
    { id: 'sys', label: 'SYSTEM', icon: Settings },
  ];

  return (
    <nav className="sticky top-[68px] md:top-[74px] z-40 bg-bg/40 backdrop-blur-md border-b border-white/10 px-2 py-2 flex gap-1.5 mb-6 rounded-2xl md:rounded-none">
      {tabs.map(t => (
        <button
          key={t.id}
          onClick={() => onTabChange(t.id)}
          className={`flex-1 flex flex-col sm:flex-row items-center justify-center gap-1 md:gap-1.5 py-2 md:py-3 rounded-xl font-black transition-all duration-300 ${
            activeTab === t.id 
              ? 'bg-accent/10 border border-accent/30 text-accent shadow-[0_0_15px_rgba(99,179,237,0.1)]' 
              : 'bg-white/[0.03] text-gray-500 hover:text-gray-400 border border-transparent'
          }`}
        >
          <t.icon size={16} className={activeTab === t.id ? 'animate-pulse' : ''} />
          <span className="text-[8px] md:text-[10px] tracking-tighter sm:tracking-widest uppercase">
            {t.label}
          </span>
        </button>
      ))}
    </nav>
  );
};
