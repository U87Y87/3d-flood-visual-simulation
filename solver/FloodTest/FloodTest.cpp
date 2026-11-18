#define CPPHTTPLIB_MULTIPART_FORM_DATA

#include "../third_party/httplib.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "CAFloodNEW.h"
#include "miniz.h"

using namespace std;
namespace fs = std::filesystem;

int count1 = 0;
std::atomic<bool> g_stop_simulation{ false };

// ===== 固定路径 =====
const fs::path BASE_PATH = fs::current_path() / "flood-work";
const fs::path SIM_PATH = BASE_PATH / "Data_dx_5m";
const fs::path UPLOAD_PATH = BASE_PATH / "uploads";

/* ================= ZIP打包 ================= */
bool zipDirectory(const std::string& folder, const std::string& zipPath)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    fs::path zp(zipPath);

    if (fs::exists(zp)) {
        fs::remove(zp);
    }

    FILE* fp = nullptr;

#ifdef _WIN32
    _wfopen_s(&fp, zp.wstring().c_str(), L"wb");
#else
    fp = fopen(zp.string().c_str(), "wb");
#endif

    if (!fp) {
        cout << "❌ zip 创建失败: " << zp << endl;
        return false;
    }

    if (!mz_zip_writer_init_cfile(&zip, fp, 0)) {
        cout << "❌ zip 初始化失败\n";
        fclose(fp);
        return false;
    }

    for (auto& entry : fs::recursive_directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;

        string relativePath = fs::relative(entry.path(), folder).string();
        replace(relativePath.begin(), relativePath.end(), '\\', '/');

        ifstream ifs(entry.path(), ios::binary | ios::ate);
        if (!ifs) continue;

        streamsize size = ifs.tellg();
        ifs.seekg(0, ios::beg);

        vector<char> buffer(size);
        if (ifs.read(buffer.data(), size)) {
            mz_zip_writer_add_mem(&zip,
                relativePath.c_str(),
                buffer.data(),
                size,
                MZ_BEST_COMPRESSION);
        }
    }

    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    fclose(fp);

    cout << "✅ zip完成: " << zipPath << endl;
    return true;
}

/* ================= 洪水模拟无转json开始 ================= */
void runFloodSimulation(const std::string& strdatapath) {

    count1 = 0;
    g_stop_simulation = false; // 重置停止标志

    cout << "\n 开始洪水模拟...\n";

    CAFlood* m_pFlood = new CAFlood();

    m_pFlood->InitCellArray((char*)strdatapath.c_str());

    DAMINFORMATION dam;
    dam.m_fDamVolume = 200000000;
    dam.m_fDamWidth = 30;
    dam.m_fDamHeight = 3;
    dam.m_fBreakWidth = 30;
    dam.m_fBreakHeight = 3;

    m_pFlood->SetDamBreakInfo(dam, 1.0, true);
    m_pFlood->ComputeRushTimeANDQm();
    m_pFlood->m_fRestVolume = dam.m_fDamVolume;

    while (true) {
        // 检测到停止信号立刻退出循环
        if (g_stop_simulation.load()) {
            cout << "⚠️ 收到前端终止信号，正在退出模拟循环...\n";
            break;
        }

        if (m_pFlood->m_fRestVolume == 0) break;

        for (int i = 0; i < 80; ++i) {
            m_pFlood->Calcultate();
        }

        int temp = (int)m_pFlood->m_fCurTime;
        int hour = temp / 3600;
        int minute = (temp % 3600) / 60;
        int second = temp % 60;

        cout << "演进时间：" << hour << "h "
            << minute << "m "
            << second << "s "
            << " 剩余水量：" << (int)m_pFlood->m_fRestVolume << endl;

        count1++;

        std::string filename =
            (fs::path(strdatapath) / (to_string(count1) + ".txt")).string();

        // ⭐ 核心：只输出水深
        m_pFlood->ExportDepth((char*)filename.c_str());
    }

    if (g_stop_simulation.load()) {
        cout << "🛑 模拟已被强制终止\n";
    }
    else {
        cout << "✅ 模拟正常完成（仅水深计算）\n";
    }

    delete m_pFlood;
}/* ================= 洪水模拟无转json结束 ================= */


/* ================= 洪水模拟含转json开始 ================= */

//void runFloodSimulation(const std::string& strdatapath)
//{
//    count1 = 0;
//
//    cout << "\n开始洪水模拟...\n";
//
//    CAFlood* m_pFlood = new CAFlood();
//
//    m_pFlood->InitCellArray((char*)strdatapath.c_str());
//
//    DAMINFORMATION dam;
//    dam.m_fDamVolume = 10000;
//    dam.m_fDamWidth = 30;
//    dam.m_fDamHeight = 3;
//    dam.m_fBreakWidth = 30;
//    dam.m_fBreakHeight = 3;
//
//    m_pFlood->SetDamBreakInfo(dam, 1.0, true);
//    m_pFlood->ComputeRushTimeANDQm();
//
//    m_pFlood->m_fRestVolume = dam.m_fDamVolume;
//
//    while (true) {
//        if (m_pFlood->m_fRestVolume == 0) break;
//
//        for (int i = 0; i < 80; ++i) {
//            m_pFlood->Calcultate();
//        }
//
//        int temp = (int)m_pFlood->m_fCurTime;
//        int hour = temp / 3600;
//        int minute = (temp % 3600) / 60;
//        int second = temp % 60;
//
//        cout << "时间: "
//            << hour << "h "
//            << minute << "m "
//            << second << "s "
//            << "剩余水量: "
//            << (int)m_pFlood->m_fRestVolume
//            << endl;
//
//        count1++;
//
//        string filename =
//            (fs::path(strdatapath) /
//                (to_string(count1) + ".txt")).string();
//
//        m_pFlood->ExportDepth((char*)filename.c_str());
//    }
//
//    cout << "✅ 模拟完成，开始转换...\n";
//
//    fs::path sim_path = fs::absolute(strdatapath);
//    fs::path exePath = sim_path.parent_path() / "GridToTin.exe";
//    fs::path demFilePath = sim_path / "dem.asc";
//
//    string cmd =
//        "\"" + exePath.string() + "\" \"" +
//        demFilePath.string() + "\"";
//
//    STARTUPINFOA si = { sizeof(si) };
//    PROCESS_INFORMATION pi;
//
//    BOOL success = CreateProcessA(
//        NULL,
//        (LPSTR)cmd.c_str(),
//        NULL, NULL,
//        FALSE,
//        0,
//        NULL,
//        sim_path.string().c_str(),
//        &si,
//        &pi
//    );
//
//    if (!success) {
//        cout << "❌ GridToTin 启动失败\n";
//        delete m_pFlood;
//        return;
//    }
//
//    WaitForSingleObject(pi.hProcess, INFINITE);
//
//    CloseHandle(pi.hProcess);
//    CloseHandle(pi.hThread);
//
//    cout << "✅ 转换完成\n";
//
//    delete m_pFlood;
//}
/*================ 洪水模拟含转json结束 ================= */

/* ================= 主函数 ================= */
int main()
{
    cout << "当前目录: " << fs::current_path() << endl;

    httplib::Server svr;

    /* ===== 上传并计算 ===== */
    svr.Post("/upload-all",
        [&](const httplib::Request& req, httplib::Response& res)
        {
            try {
                fs::remove_all(UPLOAD_PATH);
                fs::create_directories(UPLOAD_PATH);

                fs::remove_all(SIM_PATH);
                fs::create_directories(SIM_PATH);

                auto dem = req.get_file_value("dem");
                auto cao = req.get_file_value("cao");
                auto river = req.get_file_value("river");

                ofstream(UPLOAD_PATH / "dem.asc", ios::binary) << dem.content;
                ofstream(UPLOAD_PATH / "cao.asc", ios::binary) << cao.content;
                ofstream(UPLOAD_PATH / "InputRiver.dat", ios::binary) << river.content;

                fs::copy(UPLOAD_PATH, SIM_PATH,
                    fs::copy_options::recursive |
                    fs::copy_options::overwrite_existing);

                runFloodSimulation(SIM_PATH.string());

                res.set_content("模拟完成", "text/plain");
            }
            catch (const exception& e) {
                res.status = 500;
                res.set_content(e.what(), "text/plain");
            }
        });

    /* ===== 停止并重启服务 ===== */
    svr.Post("/stop", [&](const httplib::Request& req, httplib::Response& res) {
        g_stop_simulation = true; // 触发终止循环

        res.set_content("已强制停止计算，后端服务正在重启...", "text/plain");

        // 延迟 1 秒后关闭服务器（脱离主线程执行，防止阻塞当前请求）
        thread([&]() {
            this_thread::sleep_for(chrono::seconds(1));
            svr.stop(); // 调用 stop 会让 main 函数的 svr.listen() 结束，从而走到后面的重启逻辑
            }).detach();
        });

    /* ===== 下载zip ===== */
    svr.Get("/download",
        [&](const httplib::Request&, httplib::Response& res)
        {
            fs::path zipPath = BASE_PATH / "result.zip";

            if (!fs::exists(SIM_PATH)) {
                res.status = 404;
                res.set_content("目录不存在", "text/plain");
                return;
            }

            zipDirectory(SIM_PATH.string(), zipPath.string());

            ifstream ifs(zipPath, ios::binary);
            stringstream buffer;
            buffer << ifs.rdbuf();

            res.set_content(buffer.str(), "application/zip");
            res.set_header("Content-Disposition", "attachment; filename=result.zip");

            // 延迟关闭服务器
            thread([&]() {
                this_thread::sleep_for(chrono::seconds(2));
                svr.stop();
                }).detach();
        });

    cout << "🚀 http://localhost:18080\n";

    svr.listen("127.0.0.1", 18080);

    // ===== 重启自身 =====
    cout << "🔄 重启服务...\n";

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcessA(
        exePath,
        NULL,
        NULL, NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL, NULL,
        &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        cout << "❌ 重启失败: " << GetLastError() << endl;
    }

    return 0;
}