#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "license.h"
#include "rsa_cng.h"

#include <cstdint>
#include <string>

#pragma comment(lib, "comctl32.lib")

static HWND g_mc = nullptr;
static HWND g_prod = nullptr;
static HWND g_exp = nullptr;
static HWND g_out = nullptr;
static void* g_prvKey = nullptr;
static void* g_prvAlg = nullptr;

static std::wstring ExeDir() {
  wchar_t p[MAX_PATH]{};
  GetModuleFileNameW(nullptr, p, MAX_PATH);
  std::wstring s = p;
  auto pos = s.find_last_of(L"\\/");
  if (pos != std::wstring::npos) s.resize(pos);
  return s;
}

static std::string WideToUtf8(std::wstring_view w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr,
                              nullptr);
  if (n <= 0) return {};
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

static std::wstring Utf8ToWide(const std::string& u) {
  if (u.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), static_cast<int>(u.size()), nullptr, 0);
  if (n <= 0) return L"";
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, u.c_str(), static_cast<int>(u.size()), w.data(), n);
  return w;
}

static void LoadPrivateOnce() {
  if (g_prvKey) return;
  std::wstring err;
  std::wstring path = ExeDir() + L"\\private.blb";
  if (!RsaLoadPrivateKeyFromFile(path, &g_prvKey, &g_prvAlg, err)) {
    MessageBoxW(nullptr, (L"无法加载 private.blb（请先生成密钥对）：\n" + err).c_str(), L"注册机",
                MB_ICONERROR);
  }
}

static void OnGenKeys(HWND wnd) {
  RsaFreeKey(g_prvKey, g_prvAlg);
  g_prvKey = nullptr;
  g_prvAlg = nullptr;
  std::wstring err;
  if (!RsaGenerateKeyPairFiles(ExeDir(), err)) {
    MessageBoxW(wnd, (L"生成失败：\n" + err).c_str(), L"注册机", MB_ICONERROR);
    return;
  }
  MessageBoxW(wnd, L"已生成 public.blb 与 private.blb（与注册机同目录）。\n请将 public.blb 随壳分发，勿泄露 private.blb。",
              L"完成", MB_ICONINFORMATION);
}

static void OnGenLic(HWND wnd) {
  LoadPrivateOnce();
  if (!g_prvKey) return;

  wchar_t b1[512]{}, b2[256]{}, b3[64]{};
  GetWindowTextW(g_mc, b1, static_cast<int>(sizeof(b1) / sizeof(b1[0])));
  GetWindowTextW(g_prod, b2, static_cast<int>(sizeof(b2) / sizeof(b2[0])));
  GetWindowTextW(g_exp, b3, static_cast<int>(sizeof(b3) / sizeof(b3[0])));
  std::string mc = WideToUtf8(b1);
  std::string prod = WideToUtf8(b2);
  std::string exs = WideToUtf8(b3);
  while (!mc.empty() && (mc.back() == ' ' || mc.back() == '\r' || mc.back() == '\n')) mc.pop_back();
  while (!prod.empty() && (prod.back() == ' ')) prod.pop_back();

  if (mc.empty() || prod.empty() || exs.empty()) {
    MessageBoxW(wnd, L"请填写机器码、产品名、到期时间（Unix 秒）。", L"注册机", MB_ICONWARNING);
    return;
  }
  uint64_t exp = 0;
  try {
    exp = std::stoull(exs);
  } catch (...) {
    MessageBoxW(wnd, L"到期时间必须是 Unix 时间戳（秒）。", L"注册机", MB_ICONWARNING);
    return;
  }

  std::string token;
  std::string err;
  if (!LicenseBuildToken(prod, exp, mc, g_prvKey, token, err)) {
    MessageBoxW(wnd, Utf8ToWide(err).c_str(), L"生成失败", MB_ICONERROR);
    return;
  }
  SetWindowTextW(g_out, Utf8ToWide(token).c_str());
}

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE: {
      int y = 12;
      CreateWindowW(L"STATIC", L"顾客机器码：", WS_CHILD | WS_VISIBLE, 20, y, 200, 20, wnd, nullptr,
                    nullptr, nullptr);
      y += 24;
      g_mc = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, y, 520, 28, wnd, nullptr,
                             nullptr, nullptr);
      y += 40;
      CreateWindowW(L"STATIC", L"产品名（需与壳侧约定一致）：", WS_CHILD | WS_VISIBLE, 20, y, 360, 20, wnd,
                    nullptr, nullptr, nullptr);
      y += 24;
      g_prod = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"myapp", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                               20, y, 520, 28, wnd, nullptr, nullptr, nullptr);
      y += 40;
      CreateWindowW(L"STATIC", L"到期 Unix 时间（秒）：", WS_CHILD | WS_VISIBLE, 20, y, 280, 20, wnd,
                    nullptr, nullptr, nullptr);
      y += 24;
      g_exp = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                              20, y, 200, 28, wnd, nullptr, nullptr, nullptr);
      y += 40;
      CreateWindowW(L"BUTTON", L"生成密钥对（首次）", WS_CHILD | WS_VISIBLE, 20, y, 180, 32, wnd,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(2)), nullptr, nullptr);
      CreateWindowW(L"BUTTON", L"生成授权码", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 220, y, 160, 32,
                    wnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(3)), nullptr, nullptr);
      y += 44;
      CreateWindowW(L"STATIC", L"授权码（发给顾客粘贴到壳窗口）：", WS_CHILD | WS_VISIBLE, 20, y, 400, 20, wnd,
                    nullptr, nullptr, nullptr);
      y += 24;
      g_out = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                              20, y, 520, 140, wnd, nullptr, nullptr, nullptr);
      return 0;
    }
    case WM_COMMAND: {
      if (HIWORD(wp) == BN_CLICKED) {
        if (LOWORD(wp) == 2) OnGenKeys(wnd);
        if (LOWORD(wp) == 3) OnGenLic(wnd);
      }
      return 0;
    }
    case WM_DESTROY:
      RsaFreeKey(g_prvKey, g_prvAlg);
      g_prvKey = nullptr;
      g_prvAlg = nullptr;
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(wnd, msg, wp, lp);
  }
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hi;
  wc.lpszClassName = L"BsphpVmpKeygen";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&wc);

  HWND w = CreateWindowExW(0, L"BsphpVmpKeygen",
                           L"bsphp.com 验证系统安全防护学习 · 注册机（仅发行方使用）",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
                           CW_USEDEFAULT, 580, 520, nullptr, nullptr, hi, nullptr);
  ShowWindow(w, show);

  MSG m{};
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return 0;
}
