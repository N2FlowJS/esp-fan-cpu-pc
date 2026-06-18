import React, { useState, useRef, useEffect } from 'react';
import { Search, ChevronDown } from 'lucide-react';

export interface SelectOption {
  value: string;
  label: string;
  subLabel?: string;
}

interface SearchableSelectProps {
  value: string;
  onChange: (value: string) => void;
  options: SelectOption[];
  placeholder?: string;
  disabled?: boolean;
}

export const SearchableSelect: React.FC<SearchableSelectProps> = ({
  value,
  onChange,
  options,
  placeholder = "Select an option...",
  disabled = false,
}) => {
  const [isOpen, setIsOpen] = useState(false);
  const [searchTerm, setSearchTerm] = useState("");
  const dropdownRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // Close on outside click
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
        setIsOpen(false);
      }
    };
    document.addEventListener("mousedown", handleClickOutside);
    return () => document.removeEventListener("mousedown", handleClickOutside);
  }, []);

  // Focus input when opened
  useEffect(() => {
    if (isOpen && inputRef.current) {
      inputRef.current.focus();
    } else {
      setSearchTerm("");
    }
  }, [isOpen]);

  const filteredOptions = options.filter(opt => 
    opt.label.toLowerCase().includes(searchTerm.toLowerCase()) || 
    opt.value.toLowerCase().includes(searchTerm.toLowerCase()) ||
    (opt.subLabel && opt.subLabel.toLowerCase().includes(searchTerm.toLowerCase()))
  );

  const selectedOption = options.find(opt => opt.value === value);

  return (
    <div className="relative w-full" ref={dropdownRef}>
      <button
        type="button"
        disabled={disabled}
        onClick={() => setIsOpen(!isOpen)}
        className={`w-full flex items-center justify-between bg-black/40 border border-white/10 rounded-xl px-4 py-3 text-xs font-mono outline-none transition-all disabled:opacity-50 ${isOpen ? 'border-accent/40 bg-white/[0.05]' : 'hover:bg-white/[0.02]'} ${selectedOption ? 'text-white' : 'text-gray-500'}`}
      >
        <div className="flex flex-col items-start truncate overflow-hidden">
          <span className="truncate">
            {selectedOption ? selectedOption.label : placeholder}
          </span>
          {selectedOption?.subLabel && (
            <span className="text-[9px] font-bold text-gray-500 truncate mt-0.5">
              {selectedOption.subLabel}
            </span>
          )}
        </div>
        <ChevronDown size={14} className={`text-gray-600 transition-transform ${isOpen ? 'rotate-180' : ''}`} />
      </button>

      {isOpen && (
        <div className="absolute z-[100] mt-2 w-full bg-[#0a0c12] border border-white/10 rounded-xl shadow-2xl overflow-hidden animate-in fade-in slide-in-from-top-2 duration-200">
          <div className="p-2 border-b border-white/5 bg-black/40">
            <div className="relative">
              <Search size={12} className="absolute left-3 top-1/2 -translate-y-1/2 text-gray-500" />
              <input
                ref={inputRef}
                type="text"
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                placeholder="Search MAC, SSID or Vendor..."
                className="w-full bg-black/60 border border-white/5 rounded-lg pl-8 pr-3 py-2 text-[10px] font-bold text-accent outline-none focus:border-accent/30 placeholder:text-gray-600"
              />
            </div>
          </div>
          
          <div className="max-h-60 overflow-y-auto scrollbar-thin scrollbar-thumb-white/10 py-1">
            {filteredOptions.length === 0 ? (
              <div className="px-4 py-3 text-[10px] font-bold text-gray-600 text-center italic">
                No results found
              </div>
            ) : (
              filteredOptions.map((opt) => (
                <button
                  key={opt.value}
                  type="button"
                  onClick={() => {
                    onChange(opt.value);
                    setIsOpen(false);
                  }}
                  className={`w-full text-left px-4 py-2.5 hover:bg-white/[0.05] transition-colors flex flex-col ${value === opt.value ? 'bg-accent/10 border-l-2 border-accent' : 'border-l-2 border-transparent'}`}
                >
                  <span className={`text-xs font-black font-mono truncate ${value === opt.value ? 'text-accent' : 'text-gray-300'}`}>
                    {opt.label}
                  </span>
                  {opt.subLabel && (
                    <span className="text-[9px] font-bold text-gray-500 truncate mt-1">
                      {opt.subLabel}
                    </span>
                  )}
                </button>
              ))
            )}
          </div>
        </div>
      )}
    </div>
  );
};
