/** Keys typed into inputs, buttons or the table editor must not trigger global shortcuts. */
export function isTypingTarget(target: EventTarget | null): boolean {
  return (
    target instanceof HTMLInputElement ||
    target instanceof HTMLSelectElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLCanvasElement ||
    target instanceof HTMLButtonElement
  );
}
