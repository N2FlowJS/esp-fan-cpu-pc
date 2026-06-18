import React, { useState, useMemo, useEffect } from 'react';
import { ChevronUp, ChevronDown, ChevronLeft, ChevronRight } from 'lucide-react';

export interface Column<T> {
  key: string;
  header: React.ReactNode;
  sortable?: boolean;
  filterable?: boolean;
  filterPlaceholder?: string;
  width?: string;
  align?: 'left' | 'center' | 'right';
  render: (row: T) => React.ReactNode;
  sortValue?: (row: T) => any;
  filterValue?: (row: T) => string;
}

interface DataTableProps<T> {
  data: T[];
  columns: Column<T>[];
  pageSizeOptions?: number[];
  defaultPageSize?: number;
  defaultSortKey?: string;
  defaultSortDir?: 'asc' | 'desc';
  globalFilter?: (row: T) => boolean;
  onRowClick?: (row: T) => void;
  rowClassName?: (row: T) => string;
  renderMobileCard?: (row: T, index: number) => React.ReactNode;
  headerLeft?: React.ReactNode;
  headerRight?: React.ReactNode;
  footer?: (filteredCount: number, totalCount: number) => React.ReactNode;
  minWidth?: string;
  containerHeight?: string;
}

export function DataTable<T>({
  data,
  columns,
  pageSizeOptions = [10, 25, 50, 100],
  defaultPageSize = 50,
  defaultSortKey,
  defaultSortDir = 'desc',
  globalFilter,
  onRowClick,
  rowClassName,
  renderMobileCard,
  headerLeft,
  headerRight,
  footer,
  minWidth = '1100px',
  containerHeight = '600px',
}: DataTableProps<T>) {
  const [currentPage, setCurrentPage] = useState(1);
  const [pageSize, setPageSize] = useState(defaultPageSize);
  const [sortConfig, setSortConfig] = useState<{ key: string; direction: 'asc' | 'desc' } | null>(
    defaultSortKey ? { key: defaultSortKey, direction: defaultSortDir } : null
  );
  const [columnFilters, setColumnFilters] = useState<{ [key: string]: string }>({});

  const handleFilterChange = (key: string, value: string) => {
    setColumnFilters((prev) => ({ ...prev, [key]: value }));
  };

  const handleSort = (key: string) => {
    setSortConfig((prev) => {
      if (prev?.key === key) {
        return { key, direction: prev.direction === 'desc' ? 'asc' : 'desc' };
      }
      return { key, direction: 'desc' };
    });
  };

  // Extract dependencies to avoid unnecessary recalculations
  const globalFilterRef = React.useRef(globalFilter);
  useEffect(() => {
    globalFilterRef.current = globalFilter;
  }, [globalFilter]);

  const filteredAndSortedData = useMemo(() => {
    let result = [...data];

    // Global Filter
    if (globalFilterRef.current) {
      result = result.filter(globalFilterRef.current);
    }

    // Column Filters
    const activeFilters = Object.entries(columnFilters).filter(([_, val]) => val.trim() !== '');
    if (activeFilters.length > 0) {
      result = result.filter((row) => {
        return activeFilters.every(([key, val]) => {
          const col = columns.find((c) => c.key === key);
          if (!col || !col.filterable) return true;
          const rowValue = col.filterValue ? col.filterValue(row) : '';
          return rowValue.toLowerCase().includes(val.toLowerCase());
        });
      });
    }

    // Sorting
    if (sortConfig) {
      const col = columns.find((c) => c.key === sortConfig.key);
      if (col) {
        result.sort((a, b) => {
          const aVal = col.sortValue ? col.sortValue(a) : '';
          const bVal = col.sortValue ? col.sortValue(b) : '';

          if (aVal === bVal) return 0;
          if (aVal === undefined || aVal === null) return 1;
          if (bVal === undefined || bVal === null) return -1;

          const modifier = sortConfig.direction === 'asc' ? 1 : -1;
          return aVal > bVal ? 1 * modifier : -1 * modifier;
        });
      }
    }

    return result;
  }, [data, columns, sortConfig, columnFilters, globalFilter]);

  // Reset page when filters or page size change
  useEffect(() => {
    setCurrentPage(1);
  }, [columnFilters, globalFilter, pageSize]);

  const totalPages = Math.ceil(filteredAndSortedData.length / pageSize);
  const paginatedData = useMemo(() => {
    const start = (currentPage - 1) * pageSize;
    return filteredAndSortedData.slice(start, start + pageSize);
  }, [filteredAndSortedData, currentPage, pageSize]);

  const SortIcon = ({ colKey }: { colKey: string }) => {
    if (sortConfig?.key !== colKey) return null;
    return sortConfig.direction === 'asc' ? (
      <ChevronUp size={8} className="ml-1 inline" />
    ) : (
      <ChevronDown size={8} className="ml-1 inline" />
    );
  };

  return (
    <div className={`bg-[#050709] border border-white/10 rounded-2xl overflow-hidden shadow-2xl flex flex-col`} style={{ minHeight: '400px', maxHeight: containerHeight }}>
      {/* Header Controls */}
      <div className="bg-white/[0.03] px-4 py-3 border-b border-white/10 flex flex-col lg:flex-row items-center justify-between gap-3">
        <div className="flex items-center gap-3 w-full lg:w-auto">
          {headerLeft}
          
          <div className="flex items-center gap-1 bg-black/40 rounded-lg p-0.5 border border-white/5">
            <button
              disabled={currentPage === 1}
              onClick={() => setCurrentPage((p) => Math.max(1, p - 1))}
              className="p-1 rounded hover:bg-white/5 text-gray-500 hover:text-white disabled:opacity-10"
            >
              <ChevronLeft size={12} />
            </button>
            <span className="text-[9px] font-black text-gray-500 min-w-[45px] text-center">
              {currentPage} / {totalPages || 1}
            </span>
            <button
              disabled={currentPage >= totalPages}
              onClick={() => setCurrentPage((p) => Math.min(totalPages, p + 1))}
              className="p-1 rounded hover:bg-white/5 text-gray-500 hover:text-white disabled:opacity-10"
            >
              <ChevronRight size={12} />
            </button>
          </div>

          <select
            className="bg-black/60 text-[9px] font-bold border border-white/5 rounded-lg px-1.5 py-0.5 outline-none text-gray-400 focus:border-accent/40"
            value={pageSize}
            onChange={(e) => setPageSize(parseInt(e.target.value))}
          >
            {pageSizeOptions.map((v) => (
              <option key={v} value={v}>
                {v} / PAGE
              </option>
            ))}
          </select>
        </div>
        
        <div className="flex items-center gap-2 w-full lg:w-auto overflow-x-auto scrollbar-none pb-1 lg:pb-0">
           {headerRight}
           <button
              onClick={() => setColumnFilters({})}
              className="px-2 py-1 rounded bg-red/10 border border-red/20 text-red text-[8px] font-bold uppercase hover:bg-red/20 transition-colors shrink-0"
              title="Clear Column Filters"
           >
              Clear Filters
           </button>
        </div>
      </div>

      {/* Table Area */}
      <div className="flex-1 overflow-y-auto font-mono text-[10px] scrollbar-thin scrollbar-thumb-white/10 relative">
        <div className="hidden sm:block">
          <table className="w-full border-collapse table-fixed" style={{ minWidth }}>
            <thead className="sticky top-0 bg-[#050709]/95 backdrop-blur z-10 text-[9px] font-bold text-gray-600 border-b border-white/5 align-top">
              <tr className="text-left">
                {columns.map((col) => (
                  <th
                    key={col.key}
                    className={`p-2 ${col.sortable ? 'cursor-pointer hover:text-white group' : ''} ${col.align === 'center' ? 'text-center' : col.align === 'right' ? 'text-right' : 'text-left'}`}
                    style={{ width: col.width }}
                    onClick={() => col.sortable && handleSort(col.key)}
                  >
                    <div>
                      {col.header}
                      {col.sortable && <SortIcon colKey={col.key} />}
                    </div>
                    {col.filterable && (
                      <input
                        type="text"
                        placeholder={col.filterPlaceholder || "Filter..."}
                        value={columnFilters[col.key] || ""}
                        onChange={(e) => handleFilterChange(col.key, e.target.value)}
                        onClick={(e) => e.stopPropagation()}
                        className={`w-full mt-1 bg-black/60 border border-white/10 rounded px-1.5 py-0.5 text-[8px] font-mono text-white outline-none focus:border-accent/40 placeholder:text-gray-700 font-normal ${col.align === 'center' ? 'text-center' : col.align === 'right' ? 'text-right' : 'text-left'}`}
                      />
                    )}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {paginatedData.map((row, i) => (
                <tr
                  key={i}
                  onClick={() => onRowClick && onRowClick(row)}
                  className={`border-b border-white/[0.02] transition-colors group ${onRowClick ? 'cursor-pointer hover:bg-accent/5 active:bg-accent/10' : ''} ${rowClassName ? rowClassName(row) : ''}`}
                >
                  {columns.map((col) => (
                    <td
                      key={col.key}
                      className={`p-2 ${col.align === 'center' ? 'text-center' : col.align === 'right' ? 'text-right' : 'text-left'}`}
                    >
                      {col.render(row)}
                    </td>
                  ))}
                </tr>
              ))}
              {paginatedData.length === 0 && (
                <tr>
                   <td colSpan={columns.length} className="text-center p-8 text-gray-600 font-bold uppercase tracking-widest">
                      No data found
                   </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
        
        {/* Mobile View */}
        {renderMobileCard && (
          <div className="sm:hidden p-3 space-y-3">
            {paginatedData.map((row, i) => renderMobileCard(row, i))}
            {paginatedData.length === 0 && (
               <div className="text-center p-8 text-gray-600 font-bold uppercase tracking-widest">
                  No data found
               </div>
            )}
          </div>
        )}
      </div>

      {/* Footer */}
      {footer && (
        <div className="bg-white/[0.02] px-4 py-1.5 border-t border-white/5 flex justify-between items-center text-[8px] font-bold text-gray-600 uppercase tracking-widest shrink-0">
          {footer(filteredAndSortedData.length, data.length)}
        </div>
      )}
    </div>
  );
}
