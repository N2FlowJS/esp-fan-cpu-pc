import React, { useState } from 'react';
import { Zap, Target, Play, Square, Settings, Wifi, AlertTriangle } from 'lucide-react';
import { useStore } from '../../store/useStore';
import { apiStressControl } from '../../utils/api';
import { getRssiColor } from '../../utils/formatters';
import { getMacVendor } from '../../utils/macLookup';
import { SearchableSelect, SelectOption } from '../SearchableSelect';

export const StressTab: React.FC = () => {
  const { sniffer, stress } = useStore();
  const [type, setType] = useState<'deauth' | 'beacon' | 'probe'>('deauth');
  const [targetMac, setTargetMac] = useState('');
  const [clientMac, setClientMac] = useState('FF:FF:FF:FF:FF:FF');
  const [channel, setChannel] = useState(1);
  const [rate, setRate] = useState(10);
  const [loading, setLoading] = useState(false);

  const isActive = stress?.active;

  const handleToggle = async () => {
    setLoading(true);
    try {
      if (isActive) {
        await apiStressControl(false);
      } else {
        await apiStressControl(true, type, targetMac, clientMac, channel, rate);
      }
    } catch (err) {
      console.error('Stress control failed:', err);
    } finally {
      setLoading(false);
    }
  };

  const apDevices = (sniffer?.devices || []).filter(d => d.isAP);

  const targetOptions: SelectOption[] = apDevices.map(d => {
    const vendor = getMacVendor(d.mac);
    return {
      value: d.mac,
      label: d.ssid || '<Hidden SSID>',
      subLabel: `${d.mac} | CH:${d.channel} | ${devVendor(vendor)} ${d.rssi}dBm`
    };
  });

  function devVendor(v: string | undefined) {
      return v ? `[${v}]` : '';
  }

  return (
    <div className="space-y-4 animate-fade-up">
      <section className="section-card">
        <div className="flex items-center gap-2 mb-4">
          <div className="w-1.5 h-3.5 bg-red rounded-full shadow-[0_0_10px_rgba(252,129,129,0.8)]" />
          <h2 className="text-[11px] font-extrabold tracking-widest text-red uppercase italic flex items-center gap-2">
            <Zap size={12} /> AP STRESS TEST
          </h2>
          {isActive && <span className="ml-auto flex items-center gap-1.5 text-[9px] font-black text-red animate-pulse">● ATTACK ACTIVE</span>}
        </div>

        <div className="space-y-6">
          {/* Test Type Selection */}
          <div className="grid grid-cols-3 gap-2">
            {[
              { id: 'deauth', label: 'DEAUTH', icon: Target },
              { id: 'beacon', label: 'BEACON', icon: Wifi },
              { id: 'probe', label: 'PROBE', icon: Settings },
            ].map(t => (
              <button
                key={t.id}
                onClick={() => setType(t.id as any)}
                disabled={isActive}
                className={`flex flex-col items-center gap-2 p-4 rounded-2xl border transition-all ${
                  type === t.id 
                    ? 'bg-red/10 border-red/30 text-red shadow-[0_0_15px_rgba(252,129,129,0.1)]' 
                    : 'bg-white/5 border-white/10 text-gray-500 hover:text-gray-400 grayscale opacity-50'
                } ${isActive ? 'cursor-not-allowed' : ''}`}
              >
                <t.icon size={20} />
                <span className="text-[10px] font-black tracking-widest">{t.label}</span>
              </button>
            ))}
          </div>

          <div className="space-y-4">
            {type === 'deauth' && (
              <>
                <div className="space-y-2">
                  <label className="text-[9px] font-black text-gray-500 uppercase tracking-widest block">TARGET AP (BSSID)</label>
                  <SearchableSelect 
                    value={targetMac}
                    onChange={(val) => {
                      setTargetMac(val);
                      const dev = apDevices.find(d => d.mac === val);
                      if (dev?.channel) setChannel(dev.channel);
                    }}
                    options={targetOptions}
                    placeholder="-- SELECT TARGET FROM SNIFFER --"
                    disabled={isActive}
                  />
                  <input 
                    type="text" 
                    value={targetMac} 
                    onChange={e => setTargetMac(e.target.value)}
                    placeholder="OR ENTER MAC MANUALLY"
                    disabled={isActive}
                    className="w-full bg-black/20 border border-white/5 rounded-xl px-4 py-2 text-[10px] font-mono text-gray-400 outline-none focus:border-red/20 disabled:opacity-50"
                  />
                </div>

                <div className="space-y-2">
                  <label className="text-[9px] font-black text-gray-500 uppercase tracking-widest block">TARGET CLIENT</label>
                  <input 
                    type="text" 
                    value={clientMac} 
                    onChange={e => setClientMac(e.target.value)}
                    placeholder="FF:FF:FF:FF:FF:FF (Broadcast)"
                    disabled={isActive}
                    className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-mono text-white outline-none focus:border-red/40 disabled:opacity-50"
                  />
                  <p className="text-[8px] text-gray-600 font-bold italic tracking-tighter">
                    Tip: Use FF:FF:FF:FF:FF:FF to deauth all clients from the AP.
                  </p>
                </div>
              </>
            )}

            <div className="grid grid-cols-2 gap-4">
              <div className="space-y-2">
                <label className="text-[9px] font-black text-gray-500 uppercase tracking-widest block">CHANNEL</label>
                <input 
                  type="number" 
                  min="1" max="13"
                  value={channel} 
                  onChange={e => setChannel(parseInt(e.target.value))}
                  disabled={isActive}
                  className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-black text-white outline-none focus:border-red/40 disabled:opacity-50"
                />
              </div>
              <div className="space-y-2">
                <label className="text-[9px] font-black text-gray-500 uppercase tracking-widest block">RATE (PKTS/S)</label>
                <input 
                  type="number" 
                  min="1" max="100"
                  value={rate} 
                  onChange={e => setRate(parseInt(e.target.value))}
                  disabled={isActive}
                  className="w-full bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-black text-white outline-none focus:border-red/40 disabled:opacity-50"
                />
              </div>
            </div>
          </div>

          <button 
            onClick={handleToggle}
            disabled={loading || (type === 'deauth' && !targetMac)}
            className={`w-full py-5 rounded-2xl font-black tracking-[0.3em] text-[11px] transition-all flex items-center justify-center gap-3 active:scale-[0.98] disabled:opacity-50 disabled:grayscale ${
              isActive 
                ? 'bg-red text-bg shadow-[0_8px_32px_rgba(252,129,129,0.3)]' 
                : 'bg-white/5 border border-red/30 text-red hover:bg-red/10'
            }`}
          >
            {loading ? (
              <div className="w-4 h-4 border-2 border-current border-t-transparent rounded-full animate-spin" />
            ) : isActive ? (
              <><Square size={16} fill="currentColor" /> STOP ATTACK</>
            ) : (
              <><Play size={16} fill="currentColor" /> EXECUTE STRESS TEST</>
            )}
          </button>
        </div>
      </section>

      {isActive && (
        <div className="p-6 rounded-3xl bg-red/5 border border-red/20 flex flex-col items-center gap-4 text-center animate-pulse">
          <AlertTriangle className="text-red" size={24} />
          <div>
            <div className="text-xs font-black text-red uppercase tracking-widest mb-1">DANGER: STRESS TEST IN PROGRESS</div>
            <p className="text-[9px] text-red/60 font-bold uppercase tracking-tighter max-w-[200px]">
              This device is currently transmitting high-volume traffic on channel {channel}. 
              Local network connectivity may be affected.
            </p>
          </div>
        </div>
      )}

      {/* Target Info from Sniffer if available */}
      {type === 'deauth' && targetMac && apDevices.find(d => d.mac === targetMac) && (
        <div className="section-card border-accent/20">
          <div className="flex items-center gap-2 mb-3">
             <Target size={12} className="text-accent" />
             <span className="text-[10px] font-black text-gray-500 tracking-widest uppercase">TARGET PREVIEW</span>
          </div>
          {(() => {
            const dev = apDevices.find(d => d.mac === targetMac)!;
            return (
              <div className="bg-black/20 rounded-2xl p-4 space-y-2">
                <div className="flex justify-between items-center">
                  <span className="text-xs font-black text-white">{dev.ssid || '<Hidden>'}</span>
                  <span className={`text-[10px] font-black ${getRssiColor(dev.rssi)}`}>{dev.rssi} dBm</span>
                </div>
                <div className="flex gap-3 text-[9px] font-bold text-gray-600">
                  <span>{dev.mac}</span>
                  <span className="text-accent">{getMacVendor(dev.mac)}</span>
                </div>
                <div className="flex gap-2 text-[8px] font-black uppercase">
                  <span className="bg-white/5 px-2 py-0.5 rounded border border-white/5">CH {dev.channel}</span>
                  <span className="bg-white/5 px-2 py-0.5 rounded border border-white/5">{dev.security}</span>
                </div>
              </div>
            );
          })()}
        </div>
      )}
    </div>
  );
};
