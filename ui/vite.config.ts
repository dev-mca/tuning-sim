import { fileURLToPath } from 'node:url';

import tailwindcss from '@tailwindcss/vite';
import react from '@vitejs/plugin-react';
import { searchForWorkspaceRoot } from 'vite';
import { defineConfig } from 'vitest/config';

/** shared/ lives next to ui/; the fake data source imports engine tables and golden files from it. */
const sharedDir = fileURLToPath(new URL('../shared', import.meta.url));

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), tailwindcss()],
  server: {
    // The dev server only serves files below the project root unless they are allowed here.
    fs: { allow: [searchForWorkspaceRoot(process.cwd()), sharedDir] },
  },
  test: {
    include: ['src/**/*.test.ts'],
  },
});
