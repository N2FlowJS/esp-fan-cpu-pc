import React, { useState } from 'react';
import { Header } from './components/Header';
import { TabNav } from './components/TabNav';
import { DashboardTab } from './components/tabs/DashboardTab';
import { SnifferTab } from './components/tabs/SnifferTab';
import { NetworkTab } from './components/tabs/NetworkTab';
import { SystemTab } from './components/tabs/SystemTab';
import { PacketInspector } from './components/PacketInspector';
import { LoginPage } from './components/LoginPage';
import { useStore } from './store/useStore';
import { useSSE } from './hooks/useSSE';
import { PacketLog } from './types';
import { apiSetMode, apiSetSpeed, apiStep, setLedConfig } from './utils/api';

const App = () => {
  const [activeTab, setActiveTab] = useState<'dash' | 'snif' | 'sys' | 'net'>('dash');
  const [inspectPacket, setInspectPacket] = useState<{ log: PacketLog, seq: number } | null>(null);
  
  const { isAuthenticated, status, sniffer, logs, isOnline, clearLogs, setAuthenticated } = useStore();

  // SSE logic only runs if authenticated
  useSSE();
  
  if (!isAuthenticated) {
    return <LoginPage />;
  }

  // Command handlers
  const setMode = async (mode: 'manual' | 'auto') => {
    try {
      await apiSetMode(mode);
    } catch {}
  };

  const setSpeed = async (speed: number) => {
    try {
      await apiSetSpeed(speed);
    } catch {}
  };

  const stepSpeed = async (delta: number) => {
    try {
      await apiStep(delta);
    } catch {}
  };

  const handleSetLedConfig = async (mode: string, color?: string, brightness?: number) => {
    try {
      await setLedConfig(mode, color, brightness);
    } catch {}
  };

  return (
    <div className="min-h-screen bg-bg text-gray-200 selection:bg-accent/30 font-orbitron pb-10">
      <Header temp={status?.temp} rpm={status?.rpm} isOnline={isOnline} />
      
      <div className={`w-full mx-auto px-4 md:px-6 transition-all duration-500 ease-in-out ${activeTab === 'snif' ? 'max-w-7xl' : 'max-w-2xl'}`}>
        <TabNav activeTab={activeTab} onTabChange={setActiveTab} />

        <main className="pb-24">
          {activeTab === 'dash' && (
            <DashboardTab 
              status={status} 
              setMode={setMode} 
              setSpeed={setSpeed} 
              stepSpeed={stepSpeed} 
              setLedConfig={handleSetLedConfig}
            />
          )}

          {activeTab === 'snif' && (
            <SnifferTab 
              onInspect={(log, seq) => setInspectPacket({ log, seq })}
            />
          )}

          {activeTab === 'net' && (
            <NetworkTab />
          )}

          {activeTab === 'sys' && (
            <SystemTab status={status} />
          )}
        </main>
      </div>

      {inspectPacket && (
        <PacketInspector 
          packet={inspectPacket} 
          onClose={() => setInspectPacket(null)} 
        />
      )}

      {/* Persistent Footer Info */}
      <footer className="fixed bottom-0 left-0 right-0 p-4 flex justify-center pointer-events-none z-30">
        <div className="bg-bg/40 backdrop-blur-lg border border-white/5 rounded-full px-5 py-2 shadow-2xl pointer-events-auto flex items-center gap-4">
          <p className="text-[8px] font-black text-gray-600 tracking-[0.3em] uppercase">
            ESP32 CORE • SECURE SESSION
          </p>
          <button 
            onClick={() => { setAuthenticated(false); }}
            className="text-[8px] font-black text-red/40 hover:text-red tracking-widest uppercase transition-colors"
          >
            [ LOGOUT ]
          </button>
        </div>
      </footer>
    </div>
  );
};

export default App;
