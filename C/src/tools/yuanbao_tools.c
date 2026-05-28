/*
 * yuanbao_tools.c — Yuanbao platform tools for Hermes C.
 * Implements yb_search_sticker (M05) and yb_send_sticker (M07).
 * MIT License — WuBu Slermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ================================================================
 *  Sticker database — ported from gateway/platforms/yuanbao_sticker.py
 *  59 stickers with ID, name, description, package_id, dimensions
 * ================================================================ */

#define YB_STICKER_COUNT 59

typedef struct {
    const char  *sticker_id;
    const char  *name;
    const char  *description;
    const char  *package_id;
    int          width;
    int          height;
} yb_sticker_t;

static const yb_sticker_t YB_STICKERS[YB_STICKER_COUNT] = {
    {"278", "六六六", "666 厉害 牛 棒 绝了 好强 awesome", "1003", 128, 128},
    {"262", "我想开了", "想开 佛系 释怀 顿悟 看淡了 无所谓", "1003", 128, 128},
    {"130", "害羞", "腼腆 不好意思 脸红 娇羞 羞涩 捂脸", "1003", 128, 128},
    {"252", "比心", "笔芯 爱你 爱心手势 love heart 喜欢你", "1003", 128, 128},
    {"125", "委屈", "难过 想哭 可怜巴巴 瘪嘴 受伤 被欺负", "1003", 128, 128},
    {"146", "亲亲", "么么 mua 亲一下 kiss 飞吻 啵", "1003", 128, 128},
    {"131", "酷", "帅 墨镜 cool 高冷 有型 swagger", "1003", 128, 128},
    {"145", "睡", "睡觉 困 zzZ 打盹 躺平 休眠 sleepy", "1003", 128, 128},
    {"152", "发呆", "懵 愣住 放空 呆滞 出神 脑子空白", "1003", 128, 128},
    {"157", "可怜", "卖萌 求饶 委屈巴巴 弱小 拜托 眼巴巴", "1003", 128, 128},
    {"200", "摊手", "无奈 没办法 耸肩 随便 那咋整 whatever", "1003", 128, 128},
    {"213", "头大", "头疼 烦恼 郁闷 难搞 崩溃 一团乱", "1003", 128, 128},
    {"256", "吓", "害怕 惊恐 震惊 吓一跳 恐怖 怂", "1003", 128, 128},
    {"203", "吐血", "无语 崩溃 被雷 内伤 一口老血 屮", "1003", 128, 128},
    {"185", "哼", "傲娇 生气 不满 撇嘴 不理 赌气", "1003", 128, 128},
    {"220", "嘿嘿", "坏笑 猥琐笑 偷笑 憨笑 得意 你懂的", "1003", 128, 128},
    {"218", "头秃", "程序员 加班 焦虑 没头发 秃了 肝爆", "1003", 128, 128},
    {"221", "暗中观察", "窥屏 潜水 偷偷看 角落 围观 屏住呼吸", "1003", 128, 128},
    {"224", "我酸了", "嫉妒 柠檬精 羡慕 吃柠檬 眼红 恰柠檬", "1003", 128, 128},
    {"246", "打call", "应援 加油 支持 喝彩 助威 call", "1003", 128, 128},
    {"251", "庆祝", "祝贺 开心 耶 party 胜利 干杯", "1003", 128, 128},
    {"151", "奋斗", "努力 加油 拼搏 冲 干劲 卷起来", "1003", 128, 128},
    {"143", "惊讶", "震惊 哇 不敢相信 OMG 居然 这么离谱", "1003", 128, 128},
    {"144", "疑问", "问号 不懂 啥 为什么 啥情况 懵逼问", "1003", 128, 128},
    {"248", "仔细分析", "思考 推敲 认真 研究 琢磨 让我想想", "1003", 128, 128},
    {"184", "撅嘴", "嘟嘴 卖萌 不高兴 撒娇 嘴翘", "1003", 128, 128},
    {"199", "泪奔", "大哭 伤心 破防 感动哭 泪流满面 呜呜", "1003", 128, 128},
    {"276", "尊嘟假嘟", "真的假的 真假 可爱问 你骗我 是不是", "1003", 128, 128},
    {"113", "略略略", "调皮 吐舌 不服 略 气死你 鬼脸", "1003", 128, 128},
    {"180", "困", "想睡 倦 打哈欠 睁不开眼 好困啊 sleepy", "1003", 128, 128},
    {"181", "折磨", "难受 痛苦 煎熬 蚌埠住了 受不了 要命", "1003", 128, 128},
    {"182", "抠鼻", "不屑 无聊 淡定 无所谓 鄙视 挖鼻", "1003", 128, 128},
    {"183", "鼓掌", "拍手 叫好 赞同 666 喝彩 掌声", "1003", 128, 128},
    {"204", "斜眼笑", "滑稽 坏笑 doge 意味深长 阴阳怪气 嘿嘿嘿", "1003", 128, 128},
    {"216", "辣眼睛", "看不下去 cringe 毁三观 太丑了 瞎了", "1003", 128, 128},
    {"217", "哦哟", "惊讶 起哄 哇哦 有戏 不简单 哟", "1003", 128, 128},
    {"222", "吃瓜", "围观 看戏 八卦 路人 看热闹 板凳", "1003", 128, 128},
    {"225", "狗头", "doge 保命 开玩笑 滑稽 反讽 懂的都懂", "1003", 128, 128},
    {"227", "敬礼", "salute 尊重 收到 遵命 致敬 报告", "1003", 128, 128},
    {"231", "哦", "知道了 明白 敷衍 嗯 这样啊 收到", "1003", 128, 128},
    {"236", "拿到红包", "红包 谢谢老板 发财 开心 抢到了 欧气", "1003", 128, 128},
    {"239", "牛吖", "牛 厉害 强 666 佩服 大佬", "1003", 128, 128},
    {"272", "贴贴", "抱抱 亲昵 蹭蹭 亲密 靠靠 撒娇贴", "1003", 128, 128},
    {"138", "爱心", "心 love 喜欢你 红心 示爱 么么哒", "1003", 128, 128},
    {"170", "晚安", "好梦 睡了 night 早点休息 安啦 moon", "1003", 128, 128},
    {"176", "太阳", "晴天 早上好 阳光 morning 好天气 日", "1003", 128, 128},
    {"266", "柠檬", "酸 嫉妒 柠檬精 羡慕 我酸 恰柠檬", "1003", 128, 128},
    {"267", "大冤种", "倒霉 吃亏 自嘲 好心没好报 背锅 工具人", "1003", 128, 128},
    {"132", "吐了", "恶心 yue 受不了 嫌弃 想吐 生理不适", "1003", 128, 128},
    {"134", "怒", "生气 愤怒 火大 暴躁 气炸 怼", "1003", 128, 128},
    {"165", "玫瑰", "花 示爱 表白 浪漫 送你花 情人节", "1003", 128, 128},
    {"119", "凋谢", "花谢 失恋 难过 枯萎 心碎 凉了", "1003", 128, 128},
    {"159", "点赞", "赞 认同 好棒 good like 大拇指 顶", "1003", 128, 128},
    {"164", "握手", "合作 你好 商务 hello deal 成交 友好", "1003", 128, 128},
    {"163", "抱拳", "谢谢 失敬 江湖 承让 拜托 有礼", "1003", 128, 128},
    {"169", "ok", "好的 收到 没问题 okay 行 可以 懂了", "1003", 128, 128},
    {"174", "拳头", "加油 干 冲 fight 力量 击拳 硬气", "1003", 128, 128},
    {"191", "鞭炮", "过年 喜庆 爆竹 春节 噼里啪啦 红", "1003", 128, 128},
    {"258", "烟花", "庆典 漂亮 新年 嘭 绽放 节日快乐", "1003", 128, 128},
};

/* ================================================================
 *  Search scoring — ported from Python yuanbao_sticker.py
 *  Simplified: substring match, character hit ratio, ID match
 * ================================================================ */

/* Case-insensitive substring check */
static bool has_substring(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return false;
#if defined(_GNU_SOURCE) || defined(__linux__)
    return strcasestr(haystack, needle) != NULL;
#else
    /* Portable fallback */
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0) return true;
        haystack++;
    }
    return false;
#endif
}

/* Multiset character hit ratio: what fraction of needle chars appear in haystack */
static double char_hit_ratio(const char *needle, const char *haystack) {
    if (!needle || !*needle) return 0.0;
    size_t nlen = strlen(needle);
    if (nlen == 0) return 0.0;

    /* Count chars in haystack */
    int ascii_counts[256] = {0};
    const unsigned char *h = (const unsigned char *)haystack;
    while (*h) ascii_counts[*h++]++;

    double hits = 0;
    const unsigned char *n = (const unsigned char *)needle;
    for (size_t i = 0; i < nlen; i++) {
        unsigned char ch = n[i];
        if (ascii_counts[ch] > 0) {
            hits++;
            ascii_counts[ch]--;
        }
    }
    return hits / (double)nlen;
}

/* Compute a simple relevance score for a sticker against a query (0.0 - 100.0) */
static double score_sticker(const yb_sticker_t *s, const char *query) {
    double best = 0.0;

    /* Exact name match */
    if (strcmp(s->name, query) == 0)
        best = 100.0;

    /* Exact ID match */
    if (strcmp(s->sticker_id, query) == 0)
        best = 100.0;

    /* Substring in name */
    if (has_substring(s->name, query))
        best = best > 92.0 ? best : 92.0;

    /* Substring in description */
    if (has_substring(s->description, query))
        best = best > 85.0 ? best : 85.0;

    /* Name starts with query */
    if (strncmp(s->name, query, strlen(query)) == 0)
        best = best > 88.0 ? best : 88.0;

    /* Character hit ratio on name */
    double chr = char_hit_ratio(query, s->name);
    double name_score = chr * 62.0;
    if (name_score > best) best = name_score;

    /* Character hit ratio on description (weighted 0.88) */
    chr = char_hit_ratio(query, s->description);
    double desc_score = chr * 62.0 * 0.88;
    if (desc_score > best) best = desc_score;

    return best;
}

/* Compare function for qsort: descending by score */
typedef struct {
    double              score;
    const yb_sticker_t *sticker;
} scored_sticker_t;

static int cmp_scored(const void *a, const void *b) {
    const scored_sticker_t *sa = (const scored_sticker_t *)a;
    const scored_sticker_t *sb = (const scored_sticker_t *)b;
    if (sb->score > sa->score) return 1;
    if (sb->score < sa->score) return -1;
    return 0;
}

/* ================================================================
 *  M05 — yb_search_sticker tool
 * ================================================================ */

static const char *SEARCH_SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"query\":{\"type\":\"string\",\"description\":\"Search keywords for sticker name/description/ID\"},"
      "\"limit\":{\"type\":\"integer\",\"description\":\"Max results (1-50, default: 10)\",\"default\":10}"
    "},"
    "\"required\":[]"
"}";

static char *yb_search_sticker_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    /* Parse args */
    json_node_t *args = json_parse(args_json, NULL);
    if (!args) {
        return strdup("{\"error\":\"Failed to parse arguments\"}");
    }

    const char *query = json_object_get_string(args, "query", "");
    int limit = (int)json_object_get_number(args, "limit", 10);
    if (limit <= 0) limit = 10;
    if (limit > 50) limit = 50;

    json_free(args);

    /* Build scored list */
    scored_sticker_t scored[YB_STICKER_COUNT];
    int scored_count = 0;

    if (!query || !*query) {
        /* No query: return first N alphabetically by sticker_id (numeric) */
        /* Just iterate and take first N */
        for (int i = 0; i < YB_STICKER_COUNT && scored_count < limit; i++) {
            scored[scored_count].score = 0.0;
            scored[scored_count].sticker = &YB_STICKERS[i];
            scored_count++;
        }
    } else {
        for (int i = 0; i < YB_STICKER_COUNT; i++) {
            double score = score_sticker(&YB_STICKERS[i], query);
            if (score > 0.0) {
                scored[scored_count].score = score;
                scored[scored_count].sticker = &YB_STICKERS[i];
                scored_count++;
            }
        }

        /* Sort by score descending */
        qsort(scored, (size_t)scored_count, sizeof(scored_sticker_t), cmp_scored);

        /* Apply dynamic threshold (simplified Python logic) */
        if (scored_count > 0 && scored[0].score >= 12.0) {
            double floor;
            if (scored[0].score >= 22.0)
                floor = 18.0;
            else
                floor = scored[0].score * 0.5;
            if (floor < 6.0) floor = 6.0;

            int kept = 0;
            for (int i = 0; i < scored_count; i++) {
                if (scored[i].score < floor) break;
                if (kept < i) scored[kept] = scored[i];
                kept++;
            }
            scored_count = kept;
        }

        /* Cap at limit */
        if (scored_count > limit) scored_count = limit;
    }

    /* Build JSON result */
    json_node_t *results = json_new_array();
    for (int i = 0; i < scored_count; i++) {
        const yb_sticker_t *s = scored[i].sticker;
        json_node_t *item = json_new_object();
        json_object_set(item, "sticker_id", json_new_string(s->sticker_id));
        json_object_set(item, "name", json_new_string(s->name));
        json_object_set(item, "description", json_new_string(s->description));
        json_object_set(item, "package_id", json_new_string(s->package_id));
        json_array_append(results, item);
    }

    json_node_t *result = json_new_object();
    json_object_set(result, "success", json_new_bool(true));
    json_object_set(result, "query", json_new_string(query ? query : ""));
    json_object_set(result, "count", json_new_number((double)scored_count));
    json_object_set(result, "results", results);

    char *json_out = json_serialize(result);
    json_free(result);

    return json_out ? json_out : strdup("{\"error\":\"Serialization failed\"}");
}

/* ================================================================
 *  M07 — yb_send_sticker tool (needs gateway connection)
 * ================================================================ */

static const char *SEND_SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"sticker\":{\"type\":\"string\",\"description\":\"Sticker name (e.g. '六六六') or sticker_id (e.g. '278'). Empty = random\"},"
      "\"chat_id\":{\"type\":\"string\",\"description\":\"Target chat ID. Format: direct:{user_id} or group:{group_code}. Defaults to current session.\"},"
      "\"reply_to\":{\"type\":\"string\",\"description\":\"Reply message ID (optional)\"}"
    "},"
    "\"required\":[]"
"}";

static char *yb_send_sticker_handler(const char *args_json, const char *task_id) {
    (void)task_id;

    /* Parse args */
    json_node_t *args = json_parse(args_json, NULL);
    if (!args) {
        return strdup("{\"success\":false,\"error\":\"Failed to parse arguments\"}");
    }

    const char *sticker_arg = json_object_get_string(args, "sticker", "");
    const char *chat_id = json_object_get_string(args, "chat_id", "");

    json_free(args);

    if (!sticker_arg || !*sticker_arg) {
        return strdup("{\"success\":false,\"error\":\"sticker name or ID required\"}");
    }

    /* Look up the sticker by name, then by ID */
    const yb_sticker_t *found = NULL;
    for (int i = 0; i < YB_STICKER_COUNT; i++) {
        if (strcmp(YB_STICKERS[i].name, sticker_arg) == 0 ||
            strcmp(YB_STICKERS[i].sticker_id, sticker_arg) == 0) {
            found = &YB_STICKERS[i];
            break;
        }
    }
    if (!found) {
        char err[512];
        snprintf(err, sizeof(err),
                 "{\"success\":false,\"error\":\"Sticker not found: %s\","
                 "\"hint\":\"Use yb_search_sticker first to find a sticker\"}",
                 sticker_arg);
        return strdup(err);
    }

    /* Determine target chat_id */
    char to_uid[256];
    if (chat_id && *chat_id) {
        /* Extract user_id from direct:{user_id} format */
        if (strncmp(chat_id, "direct:", 7) == 0)
            snprintf(to_uid, sizeof(to_uid), "%s", chat_id + 7);
        else
            snprintf(to_uid, sizeof(to_uid), "%s", chat_id);
    } else {
        /* Try to get from session env */
        const char *env_chat = getenv("HERMES_SESSION_CHAT_ID");
        if (env_chat && strncmp(env_chat, "direct:", 7) == 0)
            snprintf(to_uid, sizeof(to_uid), "%s", env_chat + 7);
        else if (env_chat)
            snprintf(to_uid, sizeof(to_uid), "%s", env_chat);
        else
            return strdup("{\"success\":false,\"error\":\"chat_id required (no active yuanbao session)\"}");
    }

    /* Send via yuanbao gateway */
    int rc = yuanbao_send_sticker(to_uid,
                                   found->sticker_id,
                                   found->name,
                                   found->package_id,
                                   found->width,
                                   found->height);

    if (rc == 0) {
        char result[1024];
        snprintf(result, sizeof(result),
                 "{\"success\":true,\"sticker_id\":\"%s\",\"name\":\"%s\""
                 ",\"chat_id\":\"%s\""
                 ",\"note\":\"Sticker sent successfully.\"}",
                 found->sticker_id, found->name, to_uid);
        return strdup(result);
    }

    return strdup("{\"success\":false,\"error\":\"Failed to send sticker: gateway not connected or send failed\"}");
}

/* ================================================================
 *  Registry init
 * ================================================================ */

void registry_init_yuanbao_tools(void) {
    registry_register_ex("yb_search_sticker",
        "Search Yuanbao built-in stickers by keyword. Returns matching stickers "
        "with their ID, name, description for use with yb_send_sticker.",
        SEARCH_SCHEMA, "hermes-yuanbao", yb_search_sticker_handler);

    registry_register_ex("yb_send_sticker",
        "Send a sticker to a yuanbao chat. Uses TIMFaceElem protocol. "
        "Use yb_search_sticker first to find the sticker you want.",
        SEND_SCHEMA, "hermes-yuanbao", yb_send_sticker_handler);
}
