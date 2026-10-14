#define _CRT_SECURE_NO_WARNINGS
#pragma execution_character_set("utf-8")

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#define TEAM_MIN_CHARS 3
#define BUF_TEAM       256
#define BUF_B          16
#define BUF_CMD        512
#define HOTKEY_ID      1
#define VK_F1          0x70

/* ---------- UTF-8 工具 ---------- */

static int utf8_len(const char* s) {
    int n = 0;
    size_t i = 0;
    while (s[i]) {
        unsigned char c = (unsigned char)s[i];
        size_t step;
        if (c < 0x80)                step = 1;
        else if ((c & 0xE0) == 0xC0) step = 2;
        else if ((c & 0xF0) == 0xE0) step = 3;
        else if ((c & 0xF8) == 0xF0) step = 4;
        else return -1;

        for (size_t k = 1; k < step; k++) {
            if (((unsigned char)s[i + k] & 0xC0) != 0x80)
                return -1;
        }
        i += step;
        n++;
    }
    return n;
}

static int team_has_blank(const char* s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            return 1;
    }
    return 0;
}

static int process_team(const char* input, char* output, size_t out_size) {
    size_t len = strlen(input);
    if (len == 0) return -1;
    if (team_has_blank(input)) return -1;

    int chars = utf8_len(input);
    if (chars <= 0) return -1;

    if (chars >= TEAM_MIN_CHARS) {
        if (out_size < len + 1) return -1;
        memcpy(output, input, len + 1);
    }
    else {
        size_t pad = (size_t)(TEAM_MIN_CHARS - chars);
        if (out_size < len + pad + 1) return -1;
        memcpy(output, input, len);
        for (size_t i = 0; i < pad; i++)
            output[len + i] = ' ';
        output[len + pad] = '\0';
    }
    return 0;
}

/* ---------- 发送 ---------- */

static void send_char(wchar_t wc) {
    INPUT in[2];
    ZeroMemory(in, sizeof(in));

    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = 0;
    in[0].ki.wScan = wc;
    in[0].ki.dwFlags = KEYEVENTF_UNICODE;

    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = 0;
    in[1].ki.wScan = wc;
    in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, in, sizeof(INPUT));
}

static void send_enter(void) {
    INPUT in[2];
    ZeroMemory(in, sizeof(in));

    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_RETURN;
    in[0].ki.dwFlags = 0;

    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = VK_RETURN;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, in, sizeof(INPUT));
}

/* 把 UTF-8 命令预先转成宽字符，循环里直接发，省去重复转换 */
static int send_command_wide(const wchar_t* w) {
    if (!w) return -1;
    for (int i = 0; w[i] != L'\0'; i++)
        send_char(w[i]);
    send_enter();
    return 0;
}

/* ---------- 主程序 ---------- */

static void strip_newline(char* s) {
    s[strcspn(s, "\r\n")] = '\0';
}

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    char team_input[BUF_TEAM];
    char team[BUF_TEAM];
    char b_input[BUF_B];
    char command[BUF_CMD];

    printf("=== 战术小队自动建队工具 ===    Built by 三月\n");
    printf("***注意：开始前请按两下\"~\"键(Esc键下面)打开游戏控制台***\n");
    printf("步骤：\n");
    printf("  1. 输入队伍名称（至少 3 个汉字/字母/数字，不足自动补空格）\n");
    printf("  2. 输入参数值（只能为 0 或 1）\n");
    printf("  3. 切到游戏窗口，按 F1 开始 / 停止\n");
    printf("  4. 回到本窗口输入 q 退出\n\n");

    while (1) {
        /* ---- 读队伍名称 ---- */
        printf("请输入队伍名称：");
        fflush(stdout);
        if (fgets(team_input, sizeof(team_input), stdin) == NULL) break;
        strip_newline(team_input);

        if (strcmp(team_input, "q") == 0 || strcmp(team_input, "Q") == 0) {
            printf("退出程序。\n");
            break;
        }

        if (process_team(team_input, team, sizeof(team)) != 0) {
            printf("[错误] 队伍名称非法：不能为空，不能包含空格、Tab、换行。\n\n");
            continue;
        }

        /* ---- 读 0 / 1 ---- */
        printf("请输入参数值（只能为 0 或 1）：");
        fflush(stdout);
        if (fgets(b_input, sizeof(b_input), stdin) == NULL) break;
        strip_newline(b_input);

        if (strlen(b_input) != 1 ||
            (b_input[0] != '0' && b_input[0] != '1')) {
            printf("[错误] 参数值只能为 0 或 1，请重新输入！\n\n");
            continue;
        }

        /* ---- 组装命令 ---- */
        int n = snprintf(command, sizeof(command),
            "CreateSquad %s %c", team, b_input[0]);
        if (n < 0 || (size_t)n >= sizeof(command)) {
            printf("[错误] 命令过长。\n\n");
            continue;
        }

        /* ---- 转成宽字符，供循环直接使用 ---- */
        int wlen = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
        if (wlen <= 0) {
            printf("[错误] 命令转换失败。\n\n");
            continue;
        }
        wchar_t* wcmd = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
        if (!wcmd) {
            printf("[错误] 内存不足。\n\n");
            continue;
        }
        MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd, wlen);

        printf("\n已准备命令：[%s]\n", command);
        printf("按 F1 开始 / 停止连发（切到游戏窗口后再按）。\n");
        printf("输入 q 并回车可退出程序。\n\n");
        fflush(stdout);

        /* ---- 注册全局热键 F1 ---- */
        if (!RegisterHotKey(NULL, HOTKEY_ID, 0, VK_F1)) {
            printf("[错误] F1 热键注册失败，可能被其它程序占用。\n\n");
            free(wcmd);
            continue;
        }

        int running = 0;
        MSG msg;

        /* ---- 消息循环：等待 F1 / 键盘输入 q ---- */
        while (1) {
            /* 有消息就处理（F1 触发） */
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) {
                    running = !running;
                    if (running)
                        printf(">> 连发已启动，再按 F1 停止。\n");
                    else
                        printf(">> 连发已停止。\n");
                    fflush(stdout);
                }
            }

            /* 检查控制台是否有输入（q 退出） */
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 'q' || ch == 'Q') {
                    running = 0;
                    printf("\n退出程序。\n");
                    break;
                }
            }

            /* 连发中：以最快速度发送 */
            if (running) {
                send_command_wide(wcmd);
                /* 让出一点 CPU，避免 100% 占用。
                   想要更快就把 Sleep(0) 改成 Sleep(0)，
                   想要稳一点就加大到 Sleep(1) / Sleep(5)。 */
                Sleep(1);
            }
            else {
                /* 空闲时稍微休息，避免空转占满 CPU */
                Sleep(10);
            }
        }

        UnregisterHotKey(NULL, HOTKEY_ID);
        free(wcmd);
    }

    return 0;
}