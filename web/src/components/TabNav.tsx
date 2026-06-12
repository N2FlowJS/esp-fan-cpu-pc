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
    <nav className="sticky top-[68px] z-40 bg-bg/40 backdrop-blur-md border-b border-white/10 px-3 py-2 flex gap-2 mb-4">
      {tabs.map(t => (
        <button
          key={t.id}
          onClick={() => onTabChange(t.id)}
          className={`flex-1 flex items-center justify-center gap-1.5 py-2.5 rounded-lg font-bold text-[10px] tracking-wider transition-all duration-300 ${
            activeTab === t.id 
              ? 'bg-accent/10 border border-accent/30 text-accent shadow-[inset_0_0_10px_rgba(99,179,237,0.1)]' 
              : 'bg-white/[0.03] text-gray-500 hover:text-gray-400'
          }`}
        >
          <t.icon size={12} className={activeTab === t.id ? 'animate-pulse' : ''} />
          {t.label}
        </button>
      ))}
    </nav>
  );
};
