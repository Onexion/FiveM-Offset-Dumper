#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>

class FiveMStructDumper
{
private:
    DWORD m_pid = 0;
    uintptr_t m_moduleBase = 0;
    HANDLE m_hProcess = nullptr;
    uintptr_t m_worldPtr = 0;
    uintptr_t m_localPed = 0;
    uintptr_t m_localPlayerInfo = 0;
    uintptr_t m_weaponManager = 0;
    uintptr_t m_vehicle = 0;

    uintptr_t ReadPtr(uintptr_t address)
    {
        uintptr_t value = 0;
        ReadProcessMemory(m_hProcess, (LPCVOID)address, &value, sizeof(value), nullptr);
        return value;
    }

    int ReadInt(uintptr_t address)
    {
        int value = 0;
        ReadProcessMemory(m_hProcess, (LPCVOID)address, &value, sizeof(value), nullptr);
        return value;
    }

    float ReadFloat(uintptr_t address)
    {
        float value = 0;
        ReadProcessMemory(m_hProcess, (LPCVOID)address, &value, sizeof(value), nullptr);
        return value;
    }

    bool IsValidPtr(uintptr_t ptr)
    {
        return (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFFFFFF);
    }

public:
    bool FindFiveMProcess()
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &pe))
        {
            do
            {
                std::string name;
#ifdef UNICODE
                int len = WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, NULL, 0, NULL, NULL);
                if (len > 0) {
                    name.resize(len - 1);
                    WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, &name[0], len, NULL, NULL);
                }
#else
                name = pe.szExeFile;
#endif

                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                if (name.find("fivem_") != std::string::npos &&
                    (name.find("gameprocess") != std::string::npos ||
                        name.find("gtaprocess") != std::string::npos))
                {
                    m_pid = pe.th32ProcessID;
                    std::cout << "[+] Found FiveM: " << name << " (PID: " << m_pid << ")" << std::endl;
                    return true;
                }
            } while (Process32Next(snapshot, &pe));
        }
        CloseHandle(snapshot);
        return false;
    }

    bool GetModuleBase()
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        MODULEENTRY32 me;
        me.dwSize = sizeof(MODULEENTRY32);

        if (Module32First(snapshot, &me))
        {
            do
            {
                std::string moduleName(me.szModule);
                std::transform(moduleName.begin(), moduleName.end(), moduleName.begin(), ::tolower);

                if (moduleName.find("fivem_") != std::string::npos)
                {
                    m_moduleBase = (uintptr_t)me.modBaseAddr;
                    std::cout << "[+] Module base: 0x" << std::hex << m_moduleBase << std::dec << std::endl;
                    CloseHandle(snapshot);
                    return true;
                }
            } while (Module32Next(snapshot, &me));
        }
        CloseHandle(snapshot);
        return false;
    }

    bool OpenProcessHandle()
    {
        m_hProcess = OpenProcess(PROCESS_VM_READ, FALSE, m_pid);
        return (m_hProcess != nullptr);
    }

    bool FindLocalPlayer()
    {
        std::vector<uintptr_t> worldOffsets = { 0x25b14b0, 0x25C15B0, 0x257BEA0, 0x2593320 };

        for (uintptr_t offset : worldOffsets)
        {
            uintptr_t worldAddr = m_moduleBase + offset;
            uintptr_t world = ReadPtr(worldAddr);
            if (IsValidPtr(world))
            {
                m_localPed = ReadPtr(world + 0x8);
                if (IsValidPtr(m_localPed))
                {
                    std::cout << "[+] Found local ped at +0x" << std::hex << offset << std::dec << std::endl;
                    return true;
                }
            }
        }

        std::cout << "[-] Could not find local player automatically" << std::endl;
        return false;
    }

    void DumpStructureOffsets()
    {
        std::cout << "\n========== STRUKTUR OFFSETS ==========\n" << std::endl;

        if (!FindLocalPlayer())
        {
            std::cout << "[-] Cannot dump offsets - local player not found!" << std::endl;
            return;
        }

        std::cout << "[+] Local Ped address: 0x" << std::hex << m_localPed << std::dec << std::endl;

        std::cout << "\n--- CPed (Player) ---" << std::endl;

        for (uintptr_t offset = 0x1000; offset <= 0x1200; offset += 0x8)
        {
            int val = ReadInt(m_localPed + offset);
            if (val == 4 || val == 5)
            {
                std::cout << "EntityType = 0x" << std::hex << offset << std::dec << " (value: " << val << ")" << std::endl;
                break;
            }
        }

        for (uintptr_t offset = 0x280; offset <= 0x2B0; offset += 0x4)
        {
            int val = ReadInt(m_localPed + offset);
            if (val == 100 || val == 200)
            {
                std::cout << "MaxHealth = 0x" << std::hex << offset << std::dec << " (value: " << val << ")" << std::endl;
                break;
            }
        }

        int health = ReadInt(m_localPed + 0x284);
        if (health > 0 && health < 200)
            std::cout << "Health (test) = 0x284 -> " << health << std::endl;

        for (uintptr_t offset = 0x14E0; offset <= 0x1540; offset += 0x4)
        {
            int val = ReadInt(m_localPed + offset);
            if (val >= 0 && val <= 100)
            {
                std::cout << "Armor = 0x" << std::hex << offset << std::dec << " (value: " << val << ")" << std::endl;
                break;
            }
        }

        for (uintptr_t offset = 0x10A0; offset <= 0x10E0; offset += 0x8)
        {
            uintptr_t info = ReadPtr(m_localPed + offset);
            if (IsValidPtr(info))
            {
                std::cout << "PlayerInfo = 0x" << std::hex << offset << std::dec << std::endl;
                m_localPlayerInfo = info;
                break;
            }
        }

        for (uintptr_t offset = 0x10B0; offset <= 0x10F0; offset += 0x8)
        {
            uintptr_t wmg = ReadPtr(m_localPed + offset);
            if (IsValidPtr(wmg))
            {
                std::cout << "WeaponManager = 0x" << std::hex << offset << std::dec << std::endl;
                m_weaponManager = wmg;
                break;
            }
        }

        uintptr_t boneMgr = ReadPtr(m_localPed + 0x430);
        if (IsValidPtr(boneMgr))
            std::cout << "BoneManager = 0x430 (valid)" << std::endl;
        else
            std::cout << "BoneManager = 0x430 (INVALID - needs update)" << std::endl;

        for (uintptr_t offset = 0x1400; offset <= 0x1480; offset += 0x4)
        {
            int val = ReadInt(m_localPed + offset);
            if (val != 0 && val < 0xFFFFFF)
            {
                std::cout << "Possible ConfigFlags at 0x" << std::hex << offset << std::dec << " (value: " << val << ")" << std::endl;
            }
        }

        for (uintptr_t offset = 0x1400; offset <= 0x1480; offset += 0x8)
        {
            uintptr_t frag = ReadPtr(m_localPed + offset);
            if (IsValidPtr(frag))
            {
                std::cout << "Possible FragInsNmGTA at 0x" << std::hex << offset << std::dec << std::endl;
            }
        }

        for (uintptr_t offset = 0x1420; offset <= 0x1490; offset += 0x4)
        {
            int val = ReadInt(m_localPed + offset);
            if (val == 1 || val == 0)
            {
                std::cout << "Possible VisibleFlag at 0x" << std::hex << offset << std::dec << " (value: " << val << ")" << std::endl;
            }
        }

        if (IsValidPtr(m_localPlayerInfo))
        {
            std::cout << "\n--- CPlayerInfo ---" << std::endl;

            for (uintptr_t offset = 0x70; offset <= 0xF0; offset += 0x4)
            {
                int val = ReadInt(m_localPlayerInfo + offset);
                if (val > 0 && val < 65535)
                {
                    std::cout << "PlayerNetID = 0x" << std::hex << offset << std::dec << " (value: " << val << ")" << std::endl;
                    break;
                }
            }

            char name[64];
            ReadProcessMemory(m_hProcess, (LPCVOID)(m_localPlayerInfo + 0x10A8), name, sizeof(name), nullptr);
            if (name[0] != 0)
                std::cout << "PlayerName = 0x10A8 -> " << name << std::endl;
        }

        uintptr_t lastVehicle = ReadPtr(m_localPed + 0xD10);
        if (IsValidPtr(lastVehicle))
        {
            std::cout << "\n--- CVehicle (found at +0xD10) ---" << std::endl;

            uintptr_t driver = ReadPtr(lastVehicle + 0xC90);
            if (driver == m_localPed)
                std::cout << "Driver = 0xC90 (valid - points to local ped)" << std::endl;

            int doorLock = ReadInt(lastVehicle + 0x13C0);
            if (doorLock >= 0 && doorLock <= 10)
                std::cout << "DoorLock = 0x13C0 (value: " << doorLock << ")" << std::endl;

            float bodyHealth = ReadFloat(lastVehicle + 0x820);
            if (bodyHealth > 0 && bodyHealth <= 1000)
                std::cout << "VehicleBodyHealth = 0x820 (value: " << bodyHealth << ")" << std::endl;

            float tankHealth = ReadFloat(lastVehicle + 0x824);
            if (tankHealth > 0 && tankHealth <= 1000)
                std::cout << "VehicleTankHealth = 0x824 (value: " << tankHealth << ")" << std::endl;
        }

        std::cout << "\n========== Summary ==========\n" << std::endl;
        std::cout << " This Offsets is verified:" << std::endl;
    }

    void Run()
    {
        if (!FindFiveMProcess()) {
            std::cout << "[-] FiveM not running!" << std::endl;
            return;
        }
        if (!GetModuleBase()) {
            std::cout << "[-] Failed to get module base!" << std::endl;
            return;
        }
        if (!OpenProcessHandle()) {
            std::cout << "[-] Failed to open process!" << std::endl;
            return;
        }

        DumpStructureOffsets();
        CloseHandle(m_hProcess);
    }
};

int main()
{
    SetConsoleTitleA("FiveM Offset Dumper");

    std::cout << "==============================================" << std::endl;
    std::cout << "   FiveM Structure Offset Dumper" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "\n[+] FiveM must be running and you have to be on a Server!" << std::endl;
    std::cout << "[+] Press Enter to start..." << std::endl;
    std::cin.get();

    FiveMStructDumper dumper;
    dumper.Run();

    std::cout << "\n[+] Fnished! Press Enter to close..." << std::endl;
    std::cin.get();
    return 0;
}