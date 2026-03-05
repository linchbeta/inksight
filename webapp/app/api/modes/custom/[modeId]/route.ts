import { NextRequest } from "next/server";
import { proxyGet, proxyDelete } from "../../../_proxy";

export async function GET(
  _req: NextRequest,
  { params }: { params: Promise<{ modeId: string }> },
) {
  const { modeId } = await params;
  return proxyGet(`/api/modes/custom/${encodeURIComponent(modeId)}`);
}

export async function DELETE(
  _req: NextRequest,
  { params }: { params: Promise<{ modeId: string }> },
) {
  const { modeId } = await params;
  return proxyDelete(`/api/modes/custom/${encodeURIComponent(modeId)}`);
}
