import React from "react";

export type IconName =
  | "home"
  | "plug"
  | "lamp"
  | "music"
  | "palette"
  | "film"
  | "chip"
  | "activity"
  | "gear"
  | "moon"
  | "sun"
  | "monitor"
  | "search"
  | "wifi"
  | "alert"
  | "check"
  | "power"
  | "save"
  | "upload"
  | "external";

const ICON_PATHS: Record<IconName, React.ReactNode> = {
  home: (
    <>
      <path d="M4 11 12 4l8 7" />
      <path d="M6 10v9h12v-9" />
    </>
  ),
  plug: (
    <>
      <path d="M9 3v6M15 3v6" />
      <path d="M6 9h12v3a6 6 0 0 1-12 0Z" />
      <path d="M12 18v3" />
    </>
  ),
  lamp: (
    <>
      <path d="M8 3h8l-1.2 6H9.2Z" />
      <path d="M12 9v6" />
      <path d="M9 18h6" />
    </>
  ),
  music: (
    <>
      <path d="M8 5v11" />
      <path d="M8 3v2" />
      <circle cx="6" cy="17" r="2.4" />
      <circle cx="14" cy="15" r="2.4" />
      <path d="M14 15V5l6-1.5V15" />
    </>
  ),
  palette: (
    <>
      <path d="M12 3a9 9 0 1 0 0 18c1.4 0 2-1 1.6-2-.4-1 .2-1.6 1-1.6H18a3 3 0 0 0 3-3c0-5-4-11.4-9-11.4" />
      <circle cx="7.5" cy="10.5" r="1.1" />
      <circle cx="12" cy="7.5" r="1.1" />
      <circle cx="16.5" cy="10.5" r="1.1" />
    </>
  ),
  film: (
    <>
      <rect x="3" y="4" width="18" height="16" rx="2" />
      <path d="M8 4v16M16 4v16M3 9h5M3 15h5M16 9h5M16 15h5" />
    </>
  ),
  chip: (
    <>
      <rect x="6" y="6" width="12" height="12" rx="2" />
      <path d="M9 20v-2M9 6V4M15 20v-2M15 6V4M4 9h2M4 15h2M20 9h-2M20 15h-2" />
    </>
  ),
  activity: (
    <>
      <path d="M3 12h4l2.5-7 4 14 2.5-7H21" />
    </>
  ),
  gear: (
    <>
      <circle cx="12" cy="12" r="3.2" />
      <path d="M12 3v2.5M12 18.5V21M3 12h2.5M18.5 12H21M5.6 5.6l1.8 1.8M16.6 16.6l1.8 1.8M18.4 5.6l-1.8 1.8M7.4 16.6l-1.8 1.8" />
    </>
  ),
  moon: (
    <>
      <path d="M20 15.2A8 8 0 0 1 8.8 4a8 8 0 1 0 11.2 11.2Z" />
    </>
  ),
  sun: (
    <>
      <circle cx="12" cy="12" r="4" />
      <path d="M12 2.5V5M12 19v2.5M2.5 12H5M19 12h2.5M5 5l1.8 1.8M17.2 17.2 19 19M19 5l-1.8 1.8M6.8 17.2 5 19" />
    </>
  ),
  monitor: (
    <>
      <rect x="3" y="4" width="18" height="13" rx="2" />
      <path d="M8 21h8M12 17v4" />
    </>
  ),
  search: (
    <>
      <circle cx="11" cy="11" r="6.5" />
      <path d="m20 20-3.8-3.8" />
    </>
  ),
  wifi: (
    <>
      <path d="M4 9.5a11 11 0 0 1 16 0" />
      <path d="M7 13a7 7 0 0 1 10 0" />
      <path d="M10 16.5a3 3 0 0 1 4 0" />
      <circle cx="12" cy="19.3" r="0.6" />
    </>
  ),
  alert: (
    <>
      <path d="M12 4 2.8 19h18.4Z" />
      <path d="M12 10v4M12 16.8v.4" />
    </>
  ),
  check: (
    <>
      <circle cx="12" cy="12" r="8.4" />
      <path d="m8.4 12.4 2.4 2.4 4.8-5" />
    </>
  ),
  power: (
    <>
      <path d="M12 3v9" />
      <path d="M6.2 7a8.5 8.5 0 1 0 11.6 0" />
    </>
  ),
  save: (
    <>
      <path d="M4 4h12l4 4v12H4Z" />
      <path d="M8 4v6h8V4M8 20v-6h8v6" />
    </>
  ),
  upload: (
    <>
      <path d="M12 17V4M7 9l5-5 5 5" />
      <path d="M4 20h16" />
    </>
  ),
  external: (
    <>
      <path d="M14 4h6v6" />
      <path d="M20 4 11 13" />
      <path d="M20 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1h5" />
    </>
  ),
};

export function Logo({
  size = 30,
  className,
  style,
}: {
  size?: number;
  className?: string;
  style?: React.CSSProperties;
}) {
  const gid = React.useId();
  return (
    <svg
      className={className}
      width={size}
      height={size}
      viewBox="0 0 256 256"
      style={style}
      aria-hidden="true"
    >
      <defs>
        <linearGradient id={gid} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stopColor="#232831" />
          <stop offset="1" stopColor="#14171b" />
        </linearGradient>
      </defs>
      <rect x="8" y="8" width="240" height="240" rx="56" fill={`url(#${gid})`} />
      <rect x="8" y="8" width="240" height="240" rx="56" fill="none" stroke="#2e3540" strokeWidth="1.5" />
      <g fill="none" stroke="#f59a3e" strokeWidth="10" strokeLinecap="round">
        <ellipse cx="128" cy="130" rx="86" ry="86" />
        <path d="M 63 192 C 100 192 98 136 128 130 S 162 84 192 74" />
      </g>
      <circle cx="192" cy="74" r="17" fill="#ffe9cf" />
    </svg>
  );
}

export function Icon({
  name,
  size = 20,
  className,
  style,
}: {
  name: IconName;
  size?: number;
  className?: string;
  style?: React.CSSProperties;
}) {
  return (
    <svg
      className={className}
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      style={style}
    >
      {ICON_PATHS[name]}
    </svg>
  );
}