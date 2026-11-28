import { type ChangeEvent, useRef } from 'react';

import { importMsq } from '../../domain/msq/importMsq.ts';
import { tuneFileName } from '../../domain/tuneFile.ts';
import { useSnapshot, useStore } from '../../state/StoreContext.ts';
import { Button } from './Button.tsx';

/**
 * Save, load and revert the tune. Files go through the browser (download / file picker);
 * the Tauri app in iteration 12 can swap this for native dialogs without touching the store.
 */
export function TuneMenu() {
  const store = useStore();
  const { tables, hello } = useSnapshot();
  const fileInput = useRef<HTMLInputElement>(null);
  const msqInput = useRef<HTMLInputElement>(null);
  const ready = hello !== null && tables.size > 0;

  const save = (): void => {
    const now = new Date();
    const tune = store.exportTune(now);
    if (tune === null) {
      return;
    }
    const blob = new Blob([JSON.stringify(tune, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = tuneFileName(tune.engine, now);
    link.click();
    URL.revokeObjectURL(url);
    store.notify('info', `tune saved as ${link.download}`);
  };

  const load = async (event: ChangeEvent<HTMLInputElement>): Promise<void> => {
    const file = event.target.files?.[0];
    event.target.value = ''; // allow loading the same file again
    if (file === undefined) {
      return;
    }
    try {
      const changed = store.importTune(await file.text());
      store.notify(
        'info',
        changed === 0
          ? `${file.name}: same as the current tune`
          : `${file.name}: ${String(changed)} cells changed (Ctrl+Z undoes it)`,
      );
    } catch (error) {
      // TuneFileError explains what is wrong with the file; other errors are shown as they are.
      const reason = error instanceof Error ? error.message : String(error);
      store.notify('error', `${file.name}: ${reason}`);
    }
  };

  /** TunerStudio .msq (ADR 0012): converted to a tune, then loaded like a tune file. */
  const loadMsq = async (event: ChangeEvent<HTMLInputElement>): Promise<void> => {
    const file = event.target.files?.[0];
    event.target.value = '';
    if (file === undefined || hello === null) {
      return;
    }
    try {
      const { tune, warnings } = importMsq(await file.text(), hello.engine.id, tables, new Date());
      const changed = store.importTune(JSON.stringify(tune));
      const notes = warnings.length === 0 ? '' : ` · ${warnings.join(' · ')}`;
      store.notify(
        'info',
        `${file.name}: ${String(changed)} cells changed (Ctrl+Z undoes it)${notes}`,
      );
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      store.notify('error', `${file.name}: ${reason}`);
    }
  };

  return (
    <div className="flex items-center gap-1.5">
      <Button disabled={!ready} onClick={save} title="Download the editable tables as JSON">
        Save tune
      </Button>
      <Button
        disabled={!ready}
        onClick={() => fileInput.current?.click()}
        title="Load a tune file saved before"
      >
        Load tune
      </Button>
      <Button
        disabled={!ready}
        onClick={() => msqInput.current?.click()}
        title="Import VE, ignition and AFR/lambda tables from a TunerStudio .msq file"
      >
        Import .msq…
      </Button>
      <Button
        disabled={!ready}
        onClick={() => {
          store.revertToOriginal();
        }}
        title="Restore the tables as they came from the simulation (undoable)"
      >
        Revert
      </Button>
      <input
        ref={fileInput}
        type="file"
        accept="application/json,.json"
        className="hidden"
        onChange={(event) => {
          void load(event);
        }}
      />
      <input
        ref={msqInput}
        type="file"
        accept=".msq"
        className="hidden"
        onChange={(event) => {
          void loadMsq(event);
        }}
      />
    </div>
  );
}
