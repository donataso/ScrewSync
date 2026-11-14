"use client"

import { Settings, Trash2, type LucideIcon } from "lucide-react"
import type { ButtonHTMLAttributes, InputHTMLAttributes, ReactNode, SelectHTMLAttributes } from "react"

export function Panel({ title, action, children, className = "" }: { title?: string; action?: ReactNode; children: ReactNode; className?: string }) {
  return (
    <section className={`rounded-lg border border-border bg-card ${className}`}>
      {(title || action) && (
        <header className="flex items-center justify-between gap-2 border-b border-border px-4 py-2.5">
          {title && <h2 className="text-sm font-semibold tracking-wide text-foreground">{title}</h2>}
          {action}
        </header>
      )}
      <div className="p-4">{children}</div>
    </section>
  )
}

type ButtonVariant = "primary" | "secondary" | "danger" | "ghost"

export function Button({
  variant = "secondary",
  className = "",
  ...props
}: ButtonHTMLAttributes<HTMLButtonElement> & { variant?: ButtonVariant }) {
  const base = "inline-flex items-center justify-center gap-1.5 rounded-md text-sm font-medium px-3 py-1.5 transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
  const variants: Record<ButtonVariant, string> = {
    primary: "bg-primary text-primary-foreground hover:bg-primary/90",
    secondary: "bg-secondary text-secondary-foreground hover:bg-secondary/80 border border-border",
    danger: "bg-destructive text-white hover:bg-destructive/90",
    ghost: "hover:bg-muted text-foreground",
  }
  return <button className={`${base} ${variants[variant]} ${className}`} {...props} />
}

export function IconButton({
  icon: Icon,
  title,
  iconClassName = "",
  hoverClassName = "hover:bg-border!",
  className = "",
  ...props
}: ButtonHTMLAttributes<HTMLButtonElement> & { icon: LucideIcon; title: string; iconClassName?: string; hoverClassName?: string }) {
  return (
    <Button title={title} aria-label={title} className={`size-[34px] cursor-pointer p-0! ${hoverClassName} ${className}`} {...props}>
      <Icon className={`size-4 ${iconClassName}`} />
    </Button>
  )
}

export function RemoveButton({ title = "Remove", className = "", ...props }: ButtonHTMLAttributes<HTMLButtonElement>) {
  return <IconButton icon={Trash2} title={title} iconClassName="text-destructive" hoverClassName="hover:bg-destructive/10!" className={className} {...props} />
}

export function OptionsButton({ open, className = "", ...props }: ButtonHTMLAttributes<HTMLButtonElement> & { open: boolean }) {
  return (
    <IconButton icon={Settings} title={open ? "Hide options" : "Options"} aria-pressed={open} className={`${open ? "bg-border!" : ""} ${className}`} {...props} />
  )
}

export function InfoIcon({ tooltip }: { tooltip: string }) {
  return (
    <span
      title={tooltip}
      className="inline-flex h-3 w-3 shrink-0 cursor-help items-center justify-center rounded-full border border-sky-500/50 text-[9px] leading-none text-sky-600 dark:text-sky-400"
    >
      ?
    </span>
  )
}

export function Field({ label, tooltip, hint, error, children }: { label: string; tooltip?: string; hint?: string; error?: string; children: ReactNode }) {
  return (
    <label className="flex flex-col gap-1 text-sm">
      <span className="flex items-center gap-1 text-muted-foreground">
        {label}
        {tooltip && <InfoIcon tooltip={tooltip} />}
      </span>
      {children}
      {error ? <span className="text-xs text-destructive">{error}</span> : hint && <span className="text-xs text-muted-foreground">{hint}</span>}
    </label>
  )
}

export function TextInput({ className = "", ...props }: InputHTMLAttributes<HTMLInputElement>) {
  return (
    <input
      className={`rounded-md border border-input bg-background px-2.5 py-1.5 text-sm outline-none focus:ring-2 focus:ring-ring ${className}`}
      {...props}
    />
  )
}

export function Select({ className = "", children, ...props }: SelectHTMLAttributes<HTMLSelectElement>) {
  return (
    <select
      className={`rounded-md border border-input bg-background px-2.5 py-1.5 text-sm outline-none focus:ring-2 focus:ring-ring ${className}`}
      {...props}
    >
      {children}
    </select>
  )
}

export function Toggle({ checked, onChange, disabled, label }: { checked: boolean; onChange: (v: boolean) => void; disabled?: boolean; label?: string }) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      disabled={disabled}
      onClick={() => onChange(!checked)}
      className={`relative inline-flex h-5 w-9 shrink-0 items-center rounded-full transition-colors disabled:opacity-40 disabled:cursor-not-allowed ${checked ? "bg-primary" : "bg-muted"}`}
    >
      <span className="sr-only">{label}</span>
      <span className={`inline-block h-4 w-4 rounded-full bg-white shadow transition-transform ${checked ? "translate-x-4.5" : "translate-x-0.5"}`} />
    </button>
  )
}

type BadgeTone = "neutral" | "good" | "warn" | "bad" | "info"

export function Badge({ tone = "neutral", children }: { tone?: BadgeTone; children: ReactNode }) {
  const tones: Record<BadgeTone, string> = {
    neutral: "bg-muted text-muted-foreground",
    good: "bg-emerald-500/15 text-emerald-500 dark:text-emerald-400",
    warn: "bg-amber-500/15 text-amber-600 dark:text-amber-400",
    bad: "bg-red-500/15 text-red-600 dark:text-red-400",
    info: "bg-sky-500/15 text-sky-600 dark:text-sky-400",
  }
  return <span className={`inline-flex items-center gap-1 rounded-full px-2 py-0.5 text-xs font-medium ${tones[tone]}`}>{children}</span>
}

export function LockNotice({ children }: { children: ReactNode }) {
  return (
    <p className="mb-3 rounded-md border border-amber-200 bg-amber-50 px-3 py-2 text-sm text-amber-800 dark:border-amber-800 dark:bg-amber-950/30 dark:text-amber-300">
      {children}
    </p>
  )
}
