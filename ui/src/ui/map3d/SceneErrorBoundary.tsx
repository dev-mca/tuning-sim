import { Component, type ErrorInfo, type ReactNode } from 'react';

interface SceneErrorBoundaryProps {
  children: ReactNode;
}

interface SceneErrorBoundaryState {
  message: string | null;
}

/**
 * Keeps the cockpit alive when the 3D view cannot start (no WebGL, chunk failed to load).
 * React only offers this as a class component (like a try/catch around a subtree).
 */
export class SceneErrorBoundary extends Component<
  SceneErrorBoundaryProps,
  SceneErrorBoundaryState
> {
  override state: SceneErrorBoundaryState = { message: null };

  static getDerivedStateFromError(error: unknown): SceneErrorBoundaryState {
    return { message: error instanceof Error ? error.message : String(error) };
  }

  override componentDidCatch(error: unknown, info: ErrorInfo): void {
    console.error('3D view failed', error, info.componentStack);
  }

  override render(): ReactNode {
    if (this.state.message !== null) {
      return (
        <p className="py-20 text-center text-sm text-muted">
          3D view unavailable (WebGL needed): {this.state.message}
        </p>
      );
    }
    return this.props.children;
  }
}
