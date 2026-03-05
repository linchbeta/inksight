"""
FastAPI 鉴权依赖模块。

两层鉴权：
1. Device Token — 校验 X-Device-Token 请求头，保护设备相关端点
2. Admin Token — 校验 Authorization Bearer token，保护管理端点
"""
from __future__ import annotations

import hmac
import logging
import os
import re
import secrets
from datetime import datetime, timedelta, timezone
from typing import Optional

import jwt
from fastapi import Cookie, Header, HTTPException, Request, Response

from .config_store import validate_device_token, get_device_state

logger = logging.getLogger(__name__)

def _load_jwt_secret() -> str:
    env = os.environ.get("JWT_SECRET")
    if env:
        return env
    secret_file = os.path.join(os.path.dirname(__file__), "..", ".jwt_secret")
    try:
        with open(secret_file, "r") as f:
            return f.read().strip()
    except FileNotFoundError:
        s = secrets.token_urlsafe(48)
        with open(secret_file, "w") as f:
            f.write(s)
        return s

_JWT_SECRET = _load_jwt_secret()
_JWT_ALGORITHM = "HS256"
_JWT_EXPIRE_DAYS = 30
_COOKIE_NAME = "ink_session"

_MAC_RE = re.compile(r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


def validate_mac_param(mac: str) -> str:
    """校验并规范化 MAC 地址路径参数。

    返回大写 MAC，格式无效时抛出 400。
    """
    if not mac or not _MAC_RE.match(mac):
        raise HTTPException(status_code=400, detail="MAC 地址格式无效，应为 AA:BB:CC:DD:EE:FF")
    return mac.upper()


def require_admin(
    authorization: Optional[str] = Header(default=None),
) -> None:
    """FastAPI 依赖：管理端点鉴权。

    未设置 ADMIN_TOKEN 环境变量时跳过鉴权（本地开发模式）。
    """
    admin_token = os.environ.get("ADMIN_TOKEN")
    if not admin_token:
        return

    if not authorization:
        raise HTTPException(status_code=403, detail="需要管理员认证")

    parts = authorization.split(" ", 1)
    if len(parts) != 2 or parts[0] != "Bearer":
        raise HTTPException(status_code=403, detail="认证格式无效，应为: Bearer <token>")

    if not hmac.compare_digest(parts[1], admin_token):
        raise HTTPException(status_code=403, detail="管理员 Token 无效")


async def require_device_token(
    mac: str,
    x_device_token: Optional[str] = Header(default=None),
) -> bool:
    """FastAPI 依赖：校验设备 Token。

    宽限期逻辑：
    - 设备尚未存储 Token（新设备）→ 放行
    - 设备已有 Token → 请求必须携带匹配的 Token
    """
    if x_device_token:
        valid = await validate_device_token(mac, x_device_token)
        if valid:
            return True

    state = await get_device_state(mac)
    if state and state.get("auth_token"):
        logger.warning(f"[AUTH] 设备 Token 校验失败: {mac}")
        raise HTTPException(status_code=401, detail="设备 Token 无效或缺失")

    return True


def create_session_token(user_id: int, username: str) -> str:
    payload = {
        "sub": str(user_id),
        "username": username,
        "exp": datetime.now(timezone.utc) + timedelta(days=_JWT_EXPIRE_DAYS),
    }
    return jwt.encode(payload, _JWT_SECRET, algorithm=_JWT_ALGORITHM)


def decode_session_token(token: str) -> dict | None:
    try:
        return jwt.decode(token, _JWT_SECRET, algorithms=[_JWT_ALGORITHM])
    except jwt.PyJWTError:
        return None


def set_session_cookie(response: Response, token: str):
    response.set_cookie(
        key=_COOKIE_NAME,
        value=token,
        max_age=_JWT_EXPIRE_DAYS * 86400,
        httponly=True,
        samesite="lax",
        path="/",
    )


def clear_session_cookie(response: Response):
    response.delete_cookie(key=_COOKIE_NAME, path="/")


def _extract_user(
    ink_session: Optional[str],
    request: Request,
) -> dict | None:
    for source in (
        ink_session,
        (request.headers.get("authorization", "")[7:]
         if request.headers.get("authorization", "").startswith("Bearer ") else None),
    ):
        if not source:
            continue
        payload = decode_session_token(source)
        if payload and "sub" in payload:
            return payload
    return None


async def require_user(
    request: Request,
    ink_session: Optional[str] = Cookie(default=None),
) -> int:
    payload = _extract_user(ink_session, request)
    if not payload:
        raise HTTPException(status_code=401, detail="请先登录")
    return int(payload["sub"])


async def optional_user(
    request: Request,
    ink_session: Optional[str] = Cookie(default=None),
) -> int | None:
    payload = _extract_user(ink_session, request)
    return int(payload["sub"]) if payload else None
