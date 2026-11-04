import { readDeviceName } from "./device-name.mjs"

/** @type {import('next').NextConfig} */

const DEVICE_URL = process.env.DEVICE_URL || "http://192.168.4.1"

const nextConfig = {
  distDir: "../.build/web",
  typescript: {
    ignoreBuildErrors: true,
  },
  images: {
    unoptimized: true,
  },
  webpack(config, { webpack }) {
    config.plugins.push(new webpack.DefinePlugin({ __DEVICE_NAME__: JSON.stringify(readDeviceName()) }))
    return config
  },
  async rewrites() {
    return [
      { source: "/ws", destination: `${DEVICE_URL}/ws` },
      { source: "/log", destination: `${DEVICE_URL}/log` },
      { source: "/update", destination: `${DEVICE_URL}/update` },
      { source: "/update-display", destination: `${DEVICE_URL}/update-display` },
      { source: "/wifi", destination: `${DEVICE_URL}/wifi` },
      { source: "/threads", destination: `${DEVICE_URL}/threads` },
    ]
  },
}

export default nextConfig
