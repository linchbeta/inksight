import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async rewrites() {
    const backend = process.env.INKSIGHT_BACKEND_API_BASE?.replace(/\/$/, "") || "http://127.0.0.1:8080";
    return [
      {
        source: "/api/discovery",
        destination: `${backend}/api/devices/recent`,
      },
      {
        source: "/api/auth/:path*",
        destination: `${backend}/api/auth/:path*`,
      },
      {
        source: "/api/user/:path*",
        destination: `${backend}/api/user/:path*`,
      },
    ];
  },
};

export default nextConfig;
