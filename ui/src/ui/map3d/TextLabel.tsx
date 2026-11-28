import { useEffect, useMemo } from 'react';
import { CanvasTexture, SpriteMaterial, SRGBColorSpace } from 'three';

/** Rendered once per label into a canvas; large enough to stay sharp when zoomed in. */
const TEXT_PIXELS = 64;
const PADDING_PIXELS = 8;
const LABEL_FONT_FAMILY = '"JetBrains Mono Variable", ui-monospace, monospace';

/** Horizontal anchor: which side of the text sits at `position`. */
export type LabelAnchor = 'left' | 'center' | 'right';

const ANCHOR_CENTER_X: Record<LabelAnchor, number> = { left: 0, center: 0.5, right: 1 };

interface TextLabelProps {
  text: string;
  position: [number, number, number];
  /** Text height in world units. */
  height: number;
  color: string;
  anchor?: LabelAnchor;
  bold?: boolean;
}

/**
 * Text that always faces the camera (a sprite with the text drawn into a canvas texture).
 * Real 3D text would need a font loader and an extra library; a handful of axis labels does not.
 */
export function TextLabel({
  text,
  position,
  height,
  color,
  anchor = 'center',
  bold = false,
}: TextLabelProps) {
  const label = useMemo(() => createLabelMaterial(text, color, bold), [text, color, bold]);

  useEffect(
    () => () => {
      label.material.map?.dispose();
      label.material.dispose();
    },
    [label],
  );

  return (
    <sprite
      position={position}
      scale={[height * label.aspect, height, 1]}
      center={[ANCHOR_CENTER_X[anchor], 0.5]}
      material={label.material}
    />
  );
}

function createLabelMaterial(
  text: string,
  color: string,
  bold: boolean,
): { material: SpriteMaterial; aspect: number } {
  const font = `${bold ? '600 ' : ''}${String(TEXT_PIXELS)}px ${LABEL_FONT_FAMILY}`;
  const canvas = document.createElement('canvas');
  const context = canvas.getContext('2d');
  if (context === null) {
    return { material: new SpriteMaterial({ visible: false }), aspect: 1 };
  }
  context.font = font;
  canvas.width = Math.ceil(context.measureText(text).width) + 2 * PADDING_PIXELS;
  canvas.height = TEXT_PIXELS + 2 * PADDING_PIXELS;
  // Resizing the canvas resets the context state, so the font is set again.
  context.font = font;
  context.fillStyle = color;
  context.textBaseline = 'middle';
  context.fillText(text, PADDING_PIXELS, canvas.height / 2);

  const texture = new CanvasTexture(canvas);
  texture.colorSpace = SRGBColorSpace;
  const material = new SpriteMaterial({
    map: texture,
    transparent: true,
    // Axis labels stay readable even where the surface overhangs them.
    depthTest: false,
    depthWrite: false,
  });
  return { material, aspect: canvas.width / canvas.height };
}
