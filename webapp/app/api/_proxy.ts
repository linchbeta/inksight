import { NextRequest, NextResponse } from "next/server";

const backendBase =
  process.env.INKSIGHT_BACKEND_API_BASE?.replace(/\/$/, "") ||
  "http://127.0.0.1:8080";

function imageHeadersFromUpstream(res: Response, contentType: string): HeadersInit {
  const headers: Record<string, string> = { "content-type": contentType };
  const passthrough = ["x-cache-hit", "x-preview-bypass", "x-pending-refresh"];
  for (const key of passthrough) {
    const value = res.headers.get(key);
    if (value !== null) headers[key] = value;
  }
  return headers;
}

export async function proxyGet(pathWithQuery: string) {
  const target = `${backendBase}${pathWithQuery}`;
  try {
    const res = await fetch(target, { cache: "no-store" });
    const ct = res.headers.get("content-type") || "";
    if (ct.includes("image/")) {
      const buf = await res.arrayBuffer();
      return new NextResponse(buf, {
        status: res.status,
        headers: imageHeadersFromUpstream(res, ct),
      });
    }
    if (ct.includes("application/json")) {
      return NextResponse.json(await res.json(), { status: res.status });
    }
    return new NextResponse(await res.text(), {
      status: res.status,
      headers: { "content-type": ct || "text/plain" },
    });
  } catch (error) {
    const msg = error instanceof Error ? error.message : "upstream fetch failed";
    return NextResponse.json(
      { error: "upstream_unreachable", message: msg, backend: backendBase },
      { status: 503 },
    );
  }
}

export async function proxyPost(
  path: string,
  req: NextRequest,
  extraHeaders?: Record<string, string>,
) {
  const target = `${backendBase}${path}`;
  try {
    const ct = req.headers.get("content-type") || "application/json";
    const body = await req.arrayBuffer();
    const res = await fetch(target, {
      method: "POST",
      headers: { "content-type": ct, ...(extraHeaders || {}) },
      body,
    });
    const resCt = res.headers.get("content-type") || "";
    if (resCt.includes("image/")) {
      const buf = await res.arrayBuffer();
      return new NextResponse(buf, {
        status: res.status,
        headers: imageHeadersFromUpstream(res, resCt),
      });
    }
    if (resCt.includes("application/json")) {
      return NextResponse.json(await res.json(), { status: res.status });
    }
    return new NextResponse(await res.text(), {
      status: res.status,
      headers: { "content-type": resCt || "text/plain" },
    });
  } catch (error) {
    const msg = error instanceof Error ? error.message : "upstream fetch failed";
    return NextResponse.json(
      { error: "upstream_unreachable", message: msg, backend: backendBase },
      { status: 503 },
    );
  }
}

export async function proxyPut(path: string, req: NextRequest) {
  const target = `${backendBase}${path}`;
  try {
    const ct = req.headers.get("content-type") || "application/json";
    const body = await req.arrayBuffer();
    const res = await fetch(target, {
      method: "PUT",
      headers: { "content-type": ct },
      body,
    });
    const resCt = res.headers.get("content-type") || "";
    if (resCt.includes("application/json")) {
      return NextResponse.json(await res.json(), { status: res.status });
    }
    return new NextResponse(await res.text(), {
      status: res.status,
      headers: { "content-type": resCt || "text/plain" },
    });
  } catch (error) {
    const msg = error instanceof Error ? error.message : "upstream fetch failed";
    return NextResponse.json(
      { error: "upstream_unreachable", message: msg, backend: backendBase },
      { status: 503 },
    );
  }
}

export async function proxyDelete(path: string) {
  const target = `${backendBase}${path}`;
  try {
    const res = await fetch(target, { method: "DELETE" });
    const ct = res.headers.get("content-type") || "";
    if (ct.includes("application/json")) {
      return NextResponse.json(await res.json(), { status: res.status });
    }
    return new NextResponse(await res.text(), { status: res.status });
  } catch (error) {
    const msg = error instanceof Error ? error.message : "upstream fetch failed";
    return NextResponse.json(
      { error: "upstream_unreachable", message: msg, backend: backendBase },
      { status: 503 },
    );
  }
}
