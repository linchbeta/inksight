import { NextRequest } from "next/server";
import { proxyGet, proxyPost } from "../_proxy";

export async function GET() {
  return proxyGet("/api/modes");
}

export async function POST(req: NextRequest) {
  return proxyPost("/api/modes/custom", req);
}
