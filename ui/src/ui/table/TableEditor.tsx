import { type KeyboardEvent, type PointerEvent, useEffect, useRef, useState } from 'react';

import {
  type Cell,
  moveSelection,
  selectAll,
  type Selection,
  singleCell,
} from '../../domain/selection.ts';
import { cellValue, columnCount, fractionalCell, rowCount } from '../../domain/table2d.ts';
import {
  adjustCells,
  editPrecision,
  interpolateCorners,
  LARGE_STEP_FACTOR,
  setCells,
  smoothCells,
} from '../../domain/tableEdit.ts';
import type { CellValue, TableData } from '../../protocol/messages.ts';
import { useStore } from '../../state/StoreContext.ts';
import { useAnimationFrame } from '../useAnimationFrame.ts';
import {
  cellAtPixel,
  computeLayout,
  type HeatmapLayout,
  nearestCellAtPixel,
} from './heatmapLayout.ts';
import { drawCursor, drawHeatmap, prepareCanvas } from './heatmapRenderer.ts';
import { TableToolbar } from './TableToolbar.tsx';

/** Number of past operating points drawn behind the live cursor (~0.5 s at 60 fps). */
const CURSOR_TRAIL_LENGTH = 30;

interface TableEditorProps {
  table: TableData;
  original: TableData | undefined;
}

/**
 * Heatmap editor for one ECU table. The grid is drawn on a canvas (256 cells as DOM elements
 * would be slow to repaint); a second canvas on top shows the live cursor at screen rate,
 * so the grid itself is only redrawn when the table or the selection changes.
 */
export function TableEditor({ table, original }: TableEditorProps) {
  const store = useStore();
  const wrapperRef = useRef<HTMLDivElement>(null);
  const gridRef = useRef<HTMLCanvasElement>(null);
  const cursorRef = useRef<HTMLCanvasElement>(null);
  const trail = useRef<{ x: number; y: number }[]>([]);
  const dragging = useRef(false);
  const [width, setWidth] = useState(0);
  const [selection, setSelectionState] = useState<Selection>(() => singleCell({ x: 0, y: 0 }));
  // Mirror of the selection that event handlers read. State only updates on the next render;
  // the ref is current immediately, even for several events within one frame.
  const selectionRef = useRef(selection);
  const [hovered, setHovered] = useState<Cell | null>(null);
  // Canvas text does not re-render by itself when a web font finishes loading; redraw once then.
  const [fontsReady, setFontsReady] = useState(false);

  const columns = columnCount(table);
  const rows = rowCount(table);
  const layout: HeatmapLayout | null = width > 0 ? computeLayout(width, columns, rows) : null;
  const clampedSelection = clampSelection(selection, columns, rows);
  const setSelection = (next: (current: Selection) => Selection): void => {
    selectionRef.current = clampSelection(next(selectionRef.current), columns, rows);
    setSelectionState(selectionRef.current);
  };

  useEffect(() => {
    let mounted = true;
    void document.fonts.ready.then(() => {
      if (mounted) {
        setFontsReady(true);
      }
    });
    return () => {
      mounted = false;
    };
  }, []);

  useEffect(() => {
    const wrapper = wrapperRef.current;
    if (wrapper === null) {
      return;
    }
    const observer = new ResizeObserver(([entry]) => {
      if (entry !== undefined) {
        setWidth(Math.floor(entry.contentRect.width));
      }
    });
    observer.observe(wrapper);
    return () => {
      observer.disconnect();
    };
  }, []);

  useEffect(() => {
    if (layout === null || gridRef.current === null || cursorRef.current === null) {
      return;
    }
    const context = prepareCanvas(gridRef.current, layout.width, layout.height);
    prepareCanvas(cursorRef.current, layout.width, layout.height);
    if (context !== null) {
      drawHeatmap(context, layout, table, original, clampedSelection);
    }
  }, [layout, table, original, clampedSelection, fontsReady]);

  useAnimationFrame(() => {
    const canvas = cursorRef.current;
    const state = store.latestState();
    if (layout === null || canvas === null || state === null) {
      return;
    }
    trail.current.push(fractionalCell(table, state.rpm, state.map_kPa));
    if (trail.current.length > CURSOR_TRAIL_LENGTH) {
      trail.current.shift();
    }
    const context = canvas.getContext('2d');
    if (context !== null) {
      drawCursor(context, layout, trail.current);
    }
  });

  const edit = (compute: (current: TableData, selected: Selection) => CellValue[]): void => {
    if (table.editable) {
      store.editTableWith(table.id, (current) => compute(current, selectionRef.current));
    }
  };
  const step = editPrecision(table.id).step;
  const actions = {
    adjust: (delta: number) => {
      edit((current, selected) => adjustCells(current, selected, delta));
    },
    smooth: () => {
      edit(smoothCells);
    },
    interpolate: () => {
      edit(interpolateCorners);
    },
    set: (value: number) => {
      edit((current, selected) => setCells(current, selected, value));
    },
  };

  const pixelOf = (event: PointerEvent<HTMLCanvasElement>): { x: number; y: number } => {
    const bounds = event.currentTarget.getBoundingClientRect();
    return { x: event.clientX - bounds.left, y: event.clientY - bounds.top };
  };

  const onPointerDown = (event: PointerEvent<HTMLCanvasElement>): void => {
    if (layout === null) {
      return;
    }
    const pixel = pixelOf(event);
    const cell = cellAtPixel(layout, pixel.x, pixel.y);
    if (cell === null) {
      return;
    }
    event.currentTarget.setPointerCapture(event.pointerId);
    event.currentTarget.focus();
    dragging.current = true;
    setSelection((current) =>
      event.shiftKey ? { anchor: current.anchor, focus: cell } : singleCell(cell),
    );
  };

  const onPointerMove = (event: PointerEvent<HTMLCanvasElement>): void => {
    if (layout === null) {
      return;
    }
    const pixel = pixelOf(event);
    setHovered(cellAtPixel(layout, pixel.x, pixel.y));
    if (dragging.current) {
      const focus = nearestCellAtPixel(layout, pixel.x, pixel.y);
      setSelection((current) => ({ anchor: current.anchor, focus }));
    }
  };

  const onKeyDown = (event: KeyboardEvent<HTMLCanvasElement>): void => {
    const handled = handleKey(event, {
      move: (dx, dy) => {
        setSelection((current) => moveSelection(current, dx, dy, event.shiftKey, columns, rows));
      },
      adjust: actions.adjust,
      smooth: actions.smooth,
      interpolate: actions.interpolate,
      selectAll: () => {
        setSelection(() => selectAll(columns, rows));
      },
      undo: () => {
        store.undo();
      },
      redo: () => {
        store.redo();
      },
      step,
    });
    if (handled) {
      event.preventDefault();
    }
  };

  return (
    <div className="flex flex-col gap-3">
      <TableToolbar table={table} step={step} actions={actions} />
      <div ref={wrapperRef} className="relative w-full select-none">
        <canvas
          ref={gridRef}
          tabIndex={0}
          aria-label={`${table.name} table, use arrow keys to select and + or - to edit`}
          className="block cursor-cell rounded-md outline-none focus-visible:ring-2 focus-visible:ring-accent/60"
          onPointerDown={onPointerDown}
          onPointerMove={onPointerMove}
          onPointerUp={() => {
            dragging.current = false;
          }}
          onPointerLeave={() => {
            setHovered(null);
          }}
          onKeyDown={onKeyDown}
        />
        <canvas
          ref={cursorRef}
          className="pointer-events-none absolute top-0 left-0"
          aria-hidden="true"
        />
      </div>
      <CellInfo table={table} original={original} cell={hovered ?? clampedSelection.focus} />
    </div>
  );
}

interface KeyActions {
  move: (dx: number, dy: number) => void;
  adjust: (delta: number) => void;
  smooth: () => void;
  interpolate: () => void;
  selectAll: () => void;
  undo: () => void;
  redo: () => void;
  step: number;
}

/** Keyboard map of the editor; returns false for keys it does not handle. */
function handleKey(event: KeyboardEvent, actions: KeyActions): boolean {
  const command = event.ctrlKey || event.metaKey;
  const key = event.key.toLowerCase();
  if (command && key === 'z') {
    if (event.shiftKey) {
      actions.redo();
    } else {
      actions.undo();
    }
    return true;
  }
  if (command && key === 'y') {
    actions.redo();
    return true;
  }
  if (command && key === 'a') {
    actions.selectAll();
    return true;
  }
  if (command) {
    return false;
  }
  // Up means "more MAP" because the highest MAP row is drawn on top.
  const moves: Record<string, [number, number]> = {
    arrowleft: [-1, 0],
    arrowright: [1, 0],
    arrowup: [0, 1],
    arrowdown: [0, -1],
  };
  const move = moves[key];
  if (move !== undefined) {
    actions.move(move[0], move[1]);
    return true;
  }
  const adjustments: Record<string, number> = {
    '+': actions.step,
    '=': actions.step,
    '-': -actions.step,
    pageup: actions.step * LARGE_STEP_FACTOR,
    pagedown: -actions.step * LARGE_STEP_FACTOR,
  };
  const delta = adjustments[key];
  if (delta !== undefined) {
    actions.adjust(delta);
    return true;
  }
  if (key === 's') {
    actions.smooth();
    return true;
  }
  if (key === 'i') {
    actions.interpolate();
    return true;
  }
  return false;
}

function CellInfo({
  table,
  original,
  cell,
}: {
  table: TableData;
  original: TableData | undefined;
  cell: Cell;
}) {
  const value = cellValue(table, cell.x, cell.y);
  const originalValue = original === undefined ? value : cellValue(original, cell.x, cell.y);
  const decimals = editPrecision(table.id).decimals;
  return (
    <p className="font-mono text-xs text-muted tabular-nums">
      {table.xAxis.bins[cell.x]} {table.xAxis.unit} · {table.yAxis.bins[cell.y]} {table.yAxis.unit}{' '}
      ·{' '}
      <span className="text-ink">
        {value.toFixed(decimals)} {table.unit}
      </span>
      {originalValue !== value && (
        <span className="text-accent">
          {' '}
          (was {originalValue.toFixed(decimals)}, {value > originalValue ? '+' : ''}
          {(value - originalValue).toFixed(decimals)})
        </span>
      )}
    </p>
  );
}

function clampSelection(selection: Selection, columns: number, rows: number): Selection {
  const clamp = (cell: Cell): Cell => ({
    x: Math.min(cell.x, columns - 1),
    y: Math.min(cell.y, rows - 1),
  });
  return { anchor: clamp(selection.anchor), focus: clamp(selection.focus) };
}
