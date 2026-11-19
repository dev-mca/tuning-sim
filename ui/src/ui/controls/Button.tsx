import type { ButtonHTMLAttributes } from 'react';

type Variant = 'default' | 'primary' | 'danger';

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: Variant;
}

const VARIANT_CLASSES: Record<Variant, string> = {
  default: 'border-line bg-raised text-ink hover:border-muted',
  primary: 'border-accent bg-accent text-canvas hover:brightness-110',
  danger: 'border-redline/70 bg-redline/15 text-redline hover:bg-redline/25',
};

/** The one button style of the cockpit, so all controls look and behave the same. */
export function Button({
  variant = 'default',
  className = '',
  type = 'button',
  ...rest
}: ButtonProps) {
  return (
    <button
      type={type}
      className={`inline-flex h-8 min-w-8 items-center justify-center rounded-md border px-2.5 text-sm font-medium transition focus-visible:ring-2 focus-visible:ring-accent/60 focus-visible:outline-none disabled:pointer-events-none disabled:opacity-35 ${VARIANT_CLASSES[variant]} ${className}`}
      {...rest}
    />
  );
}
