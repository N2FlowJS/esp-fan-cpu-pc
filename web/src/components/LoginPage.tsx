import React, { useState } from 'react';
import { Lock, ShieldCheck, Cpu, Loader2 } from 'lucide-react';
import { useStore } from '../store/useStore';
import { apiLogin } from '../utils/api';

export const LoginPage: React.FC = () => {
  const [password, setPassword] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const { setAuthenticated, setToken } = useStore();

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault();
    setLoading(true);
    setError(null);
    try {
      const token = await apiLogin(password);
      setToken(token);
      setAuthenticated(true);
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : 'Login failed';
      setError(msg);
      setTimeout(() => setError(null), 3000);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center p-6 bg-bg relative overflow-hidden">
      {/* Background Glows */}
      <div className="absolute top-[-10%] left-[-10%] w-64 h-64 bg-accent/10 rounded-full blur-[100px]" />
      <div className="absolute bottom-[-10%] right-[-10%] w-64 h-64 bg-warm/10 rounded-full blur-[100px]" />

      <div className="w-full max-w-sm z-10">
        <div className="text-center mb-10">
          <div className="inline-flex items-center justify-center w-20 h-20 rounded-3xl bg-accent/10 border border-accent/20 mb-6 shadow-2xl shadow-accent/10">
            <Cpu size={40} className="text-accent animate-pulse" />
          </div>
          <h1 className="text-3xl font-black tracking-tighter text-white italic">
            FAN<span className="text-accent">CTRL</span> <span className="text-xs align-top not-italic text-gray-600 ml-1">v3.0</span>
          </h1>
          <p className="text-gray-500 text-xs font-bold tracking-widest uppercase mt-2">ESP32 Secure Access</p>
        </div>

        <form onSubmit={handleLogin} className="space-y-4">
          <div className="relative group">
            <div className="absolute inset-y-0 left-0 pl-4 flex items-center pointer-events-none text-gray-500 group-focus-within:text-accent transition-colors">
              <Lock size={18} />
            </div>
            <input
              type="password"
              placeholder="ACCESS KEY"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              autoComplete="new-password"
              className={`w-full bg-surface/50 border ${error ? 'border-red animate-shake' : 'border-white/10'} rounded-2xl py-4 pl-12 pr-4 text-sm font-black text-accent tracking-[0.3em] outline-none focus:border-accent/50 focus:bg-surface transition-all placeholder:text-gray-700`}
            />
          </div>

          <button
            type="submit"
            className="w-full bg-accent text-bg py-4 rounded-2xl font-black text-xs tracking-[0.2em] shadow-lg shadow-accent/20 active:scale-[0.98] transition-all flex items-center justify-center gap-2"
          >
            <ShieldCheck size={18} /> INITIALIZE SESSION
          </button>
        </form>

        <p className="text-center text-[10px] text-gray-600 font-bold tracking-widest uppercase mt-8">
          System integrity: <span className="text-green">Verified</span>
        </p>
      </div>
    </div>
  );
};
