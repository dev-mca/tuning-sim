import { Canvas, useFrame, useThree } from '@react-three/fiber';
import { useEffect, useMemo, useRef, useState } from 'react';
import { BufferAttribute, BufferGeometry, DoubleSide, type Mesh } from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { formatClean } from '../../domain/units.ts';
import type { TableData } from '../../protocol/messages.ts';
import type { SimStore } from '../../state/simStore.ts';
import { useStore } from '../../state/StoreContext.ts';
import { SURFACE } from '../theme/palette.ts';
import {
  axisLabelBins,
  axisTitle,
  DEFAULT_BOX,
  gridLineIndices,
  niceTicks,
  operatingPoint,
  surfaceGeometry,
  surfaceScales,
} from './surfaceModel.ts';
import { TextLabel } from './TextLabel.tsx';

const BOX = DEFAULT_BOX;
const CAMERA_POSITION: [number, number, number] = [9, 7.5, 14];
const CAMERA_FOV_DEG = 40;
/** Orbit around a point a bit above the floor, roughly the centre of the surface. */
const ORBIT_TARGET_HEIGHT = BOX.height * 0.35;
const MIN_DISTANCE = 6;
const MAX_DISTANCE = 30;
const MAX_LABELS_PER_AXIS = 8;
const VALUE_TICK_COUNT = 5;
const TICK_LABEL_HEIGHT = 0.4;
const TITLE_LABEL_HEIGHT = 0.45;
const LABEL_GAP = 0.3;
const MARKER_RADIUS = 0.14;
const GRID_LINE_OPACITY = 0.55;

/**
 * The 3D view of one ECU table (lazy-loaded with three.js). React re-renders it only when the
 * table changes; the live operating point is moved per frame in `useFrame`, outside React.
 */
export function SurfaceScene({ table }: { table: TableData }) {
  // Read outside <Canvas>: the store is passed down as a prop, so the scene does not depend on
  // react-three-fiber bridging React contexts into its own renderer.
  const store = useStore();
  const fontsReady = useFontsReady();
  return (
    <Canvas
      flat // no tone mapping: the vertex colours must match the 2D heatmap exactly
      dpr={[1, 2]}
      camera={{ position: CAMERA_POSITION, fov: CAMERA_FOV_DEG }}
      aria-label={`3D view of the ${table.name} table`}
    >
      <color attach="background" args={[SURFACE.panel]} />
      <TableSurface table={table} />
      <AxisFrame key={fontsReady ? 'fonts' : 'fallback'} table={table} />
      <OperatingPointMarker table={table} store={store} />
      <OrbitRig />
    </Canvas>
  );
}

function TableSurface({ table }: { table: TableData }) {
  const { surface, grid } = useMemo(() => surfaceBuffers(table), [table]);
  useEffect(
    () => () => {
      surface.dispose();
      grid.dispose();
    },
    [surface, grid],
  );
  return (
    <>
      <mesh geometry={surface}>
        {/* Unlit material, so each vertex shows exactly its heatmap colour. The polygon
            offset pushes the surface back a little, so the grid lines on it never flicker. */}
        <meshBasicMaterial
          vertexColors
          side={DoubleSide}
          polygonOffset
          polygonOffsetFactor={1}
          polygonOffsetUnits={1}
        />
      </mesh>
      <lineSegments geometry={grid}>
        <lineBasicMaterial color={SURFACE.canvas} transparent opacity={GRID_LINE_OPACITY} />
      </lineSegments>
    </>
  );
}

/** Surface mesh and its cell grid share one position buffer. */
function surfaceBuffers(table: TableData): { surface: BufferGeometry; grid: BufferGeometry } {
  const data = surfaceGeometry(table, BOX);
  const positions = new BufferAttribute(data.positions, 3);
  const surface = new BufferGeometry();
  surface.setAttribute('position', positions);
  surface.setAttribute('color', new BufferAttribute(data.colors, 3));
  surface.setIndex(new BufferAttribute(data.indices, 1));
  const grid = new BufferGeometry();
  grid.setAttribute('position', positions);
  grid.setIndex(
    new BufferAttribute(gridLineIndices(table.xAxis.bins.length, table.yAxis.bins.length), 1),
  );
  return { surface, grid };
}

/** Floor outline, the value axis at the back-left corner and all axis labels with units. */
function AxisFrame({ table }: { table: TableData }) {
  const scales = useMemo(() => surfaceScales(table, BOX), [table]);
  const frame = useMemo(() => frameGeometry(), []);
  useEffect(
    () => () => {
      frame.dispose();
    },
    [frame],
  );
  const left = -BOX.width / 2;
  const front = BOX.depth / 2;
  const back = -BOX.depth / 2;
  const tick = { height: TICK_LABEL_HEIGHT, color: SURFACE.muted };
  const title = { height: TITLE_LABEL_HEIGHT, color: SURFACE.ink, bold: true };

  return (
    <>
      <lineSegments geometry={frame}>
        <lineBasicMaterial color={SURFACE.muted} />
      </lineSegments>
      {axisLabelBins(table.xAxis.bins, MAX_LABELS_PER_AXIS).map((bin) => (
        <TextLabel
          key={`x${String(bin)}`}
          text={formatClean(bin)}
          position={[scales.x(bin), 0, front + LABEL_GAP]}
          {...tick}
        />
      ))}
      <TextLabel
        text={axisTitle(table.xAxis.name, table.xAxis.unit)}
        position={[0, 0, front + LABEL_GAP * 4]}
        {...title}
      />
      {axisLabelBins(table.yAxis.bins, MAX_LABELS_PER_AXIS).map((bin) => (
        <TextLabel
          key={`y${String(bin)}`}
          text={formatClean(bin)}
          position={[left - LABEL_GAP, 0, scales.z(bin)]}
          anchor="right"
          {...tick}
        />
      ))}
      <TextLabel
        text={axisTitle(table.yAxis.name, table.yAxis.unit)}
        position={[left - LABEL_GAP * 5, 0, 0]}
        anchor="right"
        {...title}
      />
      {niceTicks(scales.valueMin, scales.valueMax, VALUE_TICK_COUNT).map((value) => (
        <TextLabel
          key={`v${String(value)}`}
          text={formatClean(value)}
          position={[left - LABEL_GAP, scales.y(value), back]}
          anchor="right"
          {...tick}
        />
      ))}
      <TextLabel
        text={axisTitle(table.name, table.unit)}
        position={[left, BOX.height + LABEL_GAP * 2, back]}
        {...title}
      />
    </>
  );
}

/** Rectangle on the floor plus the vertical value axis at the back-left corner. */
function frameGeometry(): BufferGeometry {
  const [l, r, f, b, top] = [
    -BOX.width / 2,
    BOX.width / 2,
    BOX.depth / 2,
    -BOX.depth / 2,
    BOX.height,
  ];
  const segments = [
    [l, 0, f, r, 0, f],
    [r, 0, f, r, 0, b],
    [r, 0, b, l, 0, b],
    [l, 0, b, l, 0, f],
    [l, 0, b, l, top, b],
  ].flat();
  const geometry = new BufferGeometry();
  geometry.setAttribute('position', new BufferAttribute(Float32Array.from(segments), 3));
  return geometry;
}

/** Ball on the surface at the current rpm/MAP, with a stem down to the floor for depth. */
function OperatingPointMarker({ table, store }: { table: TableData; store: SimStore }) {
  const ball = useRef<Mesh>(null);
  const stem = useRef<Mesh>(null);

  useFrame(() => {
    const state = store.latestState();
    if (ball.current === null || stem.current === null) {
      return;
    }
    const visible = state !== null;
    ball.current.visible = visible;
    stem.current.visible = visible;
    if (state === null) {
      return;
    }
    const [x, y, z] = operatingPoint(table, state.rpm, state.map_kPa, BOX);
    ball.current.position.set(x, y, z);
    stem.current.position.set(x, y / 2, z);
    // The stem is a unit-height cylinder; a zero scale would make its matrix singular.
    stem.current.scale.set(1, Math.max(y, 1e-3), 1);
  });

  return (
    <>
      <mesh ref={ball} visible={false} renderOrder={1}>
        <sphereGeometry args={[MARKER_RADIUS, 24, 16]} />
        {/* Drawn over the surface, so the marker never disappears inside a steep slope. */}
        <meshBasicMaterial color={SURFACE.accent} depthTest={false} />
      </mesh>
      <mesh ref={stem} visible={false}>
        <cylinderGeometry args={[MARKER_RADIUS / 4, MARKER_RADIUS / 4, 1, 8]} />
        <meshBasicMaterial color={SURFACE.accent} transparent opacity={0.6} />
      </mesh>
    </>
  );
}

/** Mouse orbit (left drag), zoom (wheel) and pan (right drag) with three's OrbitControls. */
function OrbitRig() {
  const camera = useThree((three) => three.camera);
  const element = useThree((three) => three.gl.domElement);
  const controls = useRef<OrbitControls | null>(null);

  useEffect(() => {
    const orbit = new OrbitControls(camera, element);
    orbit.target.set(0, ORBIT_TARGET_HEIGHT, 0);
    orbit.enableDamping = true;
    orbit.minDistance = MIN_DISTANCE;
    orbit.maxDistance = MAX_DISTANCE;
    // Never look from below the floor: the surface is read from above.
    orbit.maxPolarAngle = Math.PI / 2;
    orbit.update();
    controls.current = orbit;
    return () => {
      orbit.dispose();
      controls.current = null;
    };
  }, [camera, element]);

  // Damping needs an update every frame to glide out after the mouse is released.
  useFrame(() => {
    controls.current?.update();
  });
  return null;
}

/** Labels are drawn into canvases; redraw them once the web font is available. */
function useFontsReady(): boolean {
  const [ready, setReady] = useState(false);
  useEffect(() => {
    let mounted = true;
    void document.fonts.ready.then(() => {
      if (mounted) {
        setReady(true);
      }
    });
    return () => {
      mounted = false;
    };
  }, []);
  return ready;
}
