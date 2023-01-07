#include "Global.hpp"

namespace Hk::Player
{
	using _FLAntiCheat = void(__stdcall*)();
	using _FLPossibleCheatingDetected = void(__stdcall* )(int iReason);
	constexpr auto AddrAntiCheat1 = 0x70120;
	constexpr auto AddrAntiCheat2 = 0x6FD20;
	constexpr auto AddrAntiCheat3 = 0x6FAF0;
	constexpr auto AddrAntiCheat4 = 0x6FAA0;
	constexpr auto PossibleCheatingDetectedAddr = 0x6F570;

	const _FLAntiCheat AntiCheat1 = (_FLAntiCheat)((char*)hModServer + AddrAntiCheat1);
	const _FLAntiCheat AntiCheat2 = (_FLAntiCheat)((char*)hModServer + AddrAntiCheat2);
	const _FLAntiCheat AntiCheat3 = (_FLAntiCheat)((char*)hModServer + AddrAntiCheat3);
	const _FLAntiCheat AntiCheat4 = (_FLAntiCheat)((char*)hModServer + AddrAntiCheat4);
	const _FLPossibleCheatingDetected FLPossibleCheatingDetected = (_FLPossibleCheatingDetected)((char*)hModServer + PossibleCheatingDetectedAddr);

	cpp::result<void, Error> AddToGroup(ClientId client, uint iGroupId)
	{
		if (!Client::IsValidClientID(client))
			return cpp::fail(Error::InvalidClientId);

		if (const uint groupId = Players.GetGroupID(client); groupId == iGroupId)
			return cpp::fail(Error::InvalidGroupId);

		CPlayerGroup* group = CPlayerGroup::FromGroupID(iGroupId);
		if (!group)
			return cpp::fail(Error::InvalidGroupId);

		group->AddMember(client);
		return {};
	}

	cpp::result<const uint, Error> GetGroupID(ClientId client)
	{
		if (!Client::IsValidClientID(client))
			return cpp::fail(Error::InvalidClientId);

		return Players.GetGroupID(client);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<const uint, Error> GetCash(const std::variant<uint, std::wstring>& player)
	{
		if (ClientId client = Hk::Client::ExtractClientID(player); client != UINT_MAX)
		{
			if (Client::IsInCharSelectMenu(client))
				return cpp::fail(Error::CharacterNotSelected);

			int cash;
			pub::Player::InspectCash(client, cash);
			return static_cast<uint>(cash);
		}

		if (!player.index())
			return cpp::fail(Error::InvalidClientId);

		const auto acc = Hk::Client::GetAccountByCharName(std::get<std::wstring>(player));
		if (acc.has_error())
			return cpp::fail(acc.error());

		auto dir = Hk::Client::GetAccountDirName(acc.value());
		
		auto file = Client::GetCharFileName(player);
		if (file.has_error())
			return cpp::fail(file.error());

		std::string scCharFile = scAcctPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl";

		FILE* fTest;
		fopen_s(&fTest, scCharFile.c_str(), "r");
		if (!fTest)
			return cpp::fail(Error::CharacterDoesNotExist);
		else
			fclose(fTest);

		if (Client::IsEncoded(scCharFile))
		{
			const std::string scCharFileNew = scCharFile + ".ini";
			if (!flc_decode(scCharFile.c_str(), scCharFileNew.c_str()))
				return cpp::fail(Error::CouldNotDecodeCharFile);

			uint cash = static_cast<uint>(IniGetI(scCharFileNew, "Player", "money", -1));
			DeleteFile(scCharFileNew.c_str());
			return cash;
		}

		return static_cast<uint>(IniGetI(scCharFile, "Player", "money", -1));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> AdjustCash(const std::variant<uint, std::wstring>& player, int iAmount)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client != UINT_MAX)
		{
			if (Client::IsInCharSelectMenu(client))
				return cpp::fail(Error::CharacterNotSelected);

			pub::Player::AdjustCash(client, iAmount);
			return {};
		}

		if (!player.index())
		{
			return cpp::fail(Error::InvalidClientId);
		}

		const auto& character = std::get<std::wstring>(player);
		const auto acc = Hk::Client::GetAccountByCharName(std::get<std::wstring>(player));
		if (acc.has_error())
			return cpp::fail(acc.error());

		auto dir = Hk::Client::GetAccountDirName(acc.value());

		const auto file = Client::GetCharFileName(character);
		if (file.has_error())
			return cpp::fail(file.error());

		std::string scCharFile = scAcctPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl";
		int iRet;
		if (Client::IsEncoded(scCharFile))
		{
			std::string scCharFileNew = scCharFile + ".ini";

			if (!flc_decode(scCharFile.c_str(), scCharFileNew.c_str()))
				return cpp::fail(Error::CouldNotDecodeCharFile);

			iRet = IniGetI(scCharFileNew, "Player", "money", -1);
			// Add a space to the value so the ini file line looks like "<key> =
			// <value>" otherwise IFSO can't decode the file correctly
			IniWrite(scCharFileNew, "Player", "money", " " + std::to_string(iRet + iAmount));

			if (!FLHookConfig::i()->general.disableCharfileEncryption)
				if (!flc_encode(scCharFileNew.c_str(), scCharFile.c_str()))
					return cpp::fail(Error::CouldNotEncodeCharFile);

			DeleteFile(scCharFileNew.c_str());
		}
		else
		{
			iRet = IniGetI(scCharFile, "Player", "money", -1);
			// Add a space to the value so the ini file line looks like "<key> =
			// <value>" otherwise IFSO can't decode the file correctly
			IniWrite(scCharFile, "Player", "money", " " + std::to_string(iRet + iAmount));
		}

		if (client != UINT_MAX)
		{ // money fix in case player logs in with this account
			bool bFound = false;
			std::wstring characterLower = ToLower(character);
			for (auto& money : ClientInfo[client].lstMoneyFix)
			{
				if (money.character == characterLower)
				{
					money.uAmount += iAmount;
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				MONEY_FIX mf;
				mf.character = characterLower;
				mf.uAmount = iAmount;
				ClientInfo[client].lstMoneyFix.push_back(mf);
			}
		}

		return {};
	}

	cpp::result<void, Error> AddCash(const std::variant<uint, std::wstring>& player, uint uAmount)
	{
		return AdjustCash(player, static_cast<int>(uAmount));
	}

	cpp::result<void, Error> RemoveCash(const std::variant<uint, std::wstring>& player, uint uAmount)
	{
		return AdjustCash(player, -static_cast<int>(uAmount));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> Kick(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client == UINT_MAX)
			return cpp::fail(Error::PlayerNotLoggedIn);

		CAccount* acc = Players.FindAccountFromClientID(client);
		acc->ForceLogout();
		return {};
	}

	cpp::result<void, Error> KickReason(const std::variant<uint, std::wstring>& player, const std::wstring& wscReason)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client == UINT_MAX)
			return cpp::fail(Error::PlayerNotLoggedIn);

		if (wscReason.length())
			MsgAndKick(client, wscReason, FLHookConfig::i()->msgStyle.kickMsgPeriod);
		else
			Players.FindAccountFromClientID(client)->ForceLogout();

		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> Ban(const std::variant<uint, std::wstring>& player, bool bBan)
	{
		auto acc = Hk::Client::ExtractAccount(player);
		if (acc.has_error())
			return cpp::fail(Error::CharacterDoesNotExist);

		auto id = Client::GetAccountID(acc.value());
		if (id.has_error())
			return cpp::fail(id.error());

		st6::wstring flStr((ushort*)id.value().c_str());
		Players.BanAccount(flStr, bBan);
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> Beam(const std::variant<uint, std::wstring>& player, const std::variant<uint, std::wstring>& baseVar)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		uint iBaseId;

		// check if logged in
		if (client == UINT_MAX)
			return cpp::fail(Error::PlayerNotLoggedIn);

		//if basename was passed as string
		if (baseVar.index() == 1) {
			const std::string baseName = wstos(std::get<std::wstring>(baseVar));
			// check if ship in space
			uint ship = 0;
			pub::Player::GetShip(client, ship);
			if (!ship)
				return cpp::fail(Error::PlayerNotInSpace);

			// get base id
			if (pub::GetBaseID(iBaseId, baseName.c_str()) == -4)
			{
				return cpp::fail(Error::InvalidBase);
			}
		}
		else
		{
			iBaseId = std::get<uint>(baseVar);
		}
		
		uint iSysId;
		pub::Player::GetSystem(client, iSysId);
		Universe::IBase* base = Universe::get_base(iBaseId);

		pub::Player::ForceLand(client, iBaseId); // beam
		
		// if not in the same system, emulate F1 charload
		if (base->systemId != iSysId)
		{
			Server.BaseEnter(iBaseId, client);
			Server.BaseExit(iBaseId, client);
			auto fileName = Client::GetCharFileName(client);
			if (fileName.has_error())
				return cpp::fail(fileName.error());

			const std::wstring newFile = fileName.value() + L".fl";
			CHARACTER_ID cId;
			strcpy_s(cId.szCharFilename, wstos(newFile.substr(0, 14)).c_str());
			Server.CharacterSelect(cId, client);
		}
		
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> SaveChar(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client == UINT_MAX)
			return cpp::fail(Error::PlayerNotLoggedIn);

		void* pJmp = (char*)hModServer + 0x7EFA8;
		char szNop[2] = {'\x90', '\x90'};
		char szTestAlAl[2] = {'\x74', '\x44'};
		WriteProcMem(pJmp, szNop, sizeof(szNop)); // nop the SinglePlayer() check
		pub::Save(client, 1);
		WriteProcMem(pJmp, szTestAlAl, sizeof(szTestAlAl)); // restore

		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct EQ_ITEM
	{
		EQ_ITEM* next;
		uint i2;
		ushort s1;
		ushort sId;
		uint iGoodId;
		CacheString hardpoint;
		bool bMounted;
		char sz[3];
		float fStatus;
		uint iCount;
		bool bMission;
	};

	cpp::result<const std::list<CARGO_INFO>, Error> EnumCargo(const std::variant<uint, std::wstring>& player, int& iRemainingHoldSize)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client == UINT_MAX || Client::IsInCharSelectMenu(client))
			return cpp::fail(Error::PlayerNotLoggedIn);

		std::list<CARGO_INFO> cargo;

		char* szClassPtr;
		memcpy(&szClassPtr, &Players, 4);
		szClassPtr += 0x418 * (client - 1);

		EQ_ITEM* eqLst;
		memcpy(&eqLst, szClassPtr + 0x27C, 4);
		EQ_ITEM* eq;
		eq = eqLst->next;
		while (eq != eqLst)
		{
			CARGO_INFO ci = {eq->sId, (int)eq->iCount, eq->iGoodId, eq->fStatus, eq->bMission, eq->bMounted, eq->hardpoint};
			cargo.push_back(ci);

			eq = eq->next;
		}

		float fRemHold;
		pub::Player::GetRemainingHoldSize(client, fRemHold);
		iRemainingHoldSize = (int)fRemHold;
		return cargo;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> RemoveCargo(const std::variant<uint, std::wstring>& player, uint iId, int iCount)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client == -1 || Hk::Client::IsInCharSelectMenu(client))
			return cpp::fail(Error::PlayerNotLoggedIn);

		int iHold;
		const auto cargo = EnumCargo(player, iHold);
		if (cargo.has_error())
		{
			return cpp::fail(cargo.error());
		}

		for (auto& item : cargo.value())
		{
			if ((item.iId == iId) && (item.iCount < iCount))
				iCount = item.iCount; // trying to remove more than actually there
		}

		pub::Player::RemoveCargo(client, iId, iCount);
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> AddCargo(const std::variant<uint, std::wstring>& player, uint iGoodId, int iCount, bool bMission)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if (client == -1 || Hk::Client::IsInCharSelectMenu(client))
			return cpp::fail(Error::PlayerNotLoggedIn);

		// add
		const GoodInfo* gi;
		if (!(gi = GoodList::find_by_id(iGoodId)))
			return cpp::fail(Error::InvalidGood);

		bool bMultiCount;
		memcpy(&bMultiCount, (char*)gi + 0x70, 1);

		uint iBase = 0;
		pub::Player::GetBase(client, iBase);
		uint iLocation = 0;
		pub::Player::GetLocation(client, iLocation);

		// trick cheat detection
		if (iBase)
		{
			if (iLocation)
				Server.LocationExit(iLocation, client);
			Server.BaseExit(iBase, client);
			if (!Hk::Client::IsValidClientID(client)) // got cheat kicked
				return cpp::fail(Error::PlayerNotLoggedIn);
		}

		if (bMultiCount)
		{ 
			// it's a good that can have multiple units(commodities missile ammo, etc)
			int ret;

			// we need to do this, else server or client may crash
			const auto cargo = EnumCargo(player, ret);
			for (auto& item : cargo.value())
			{
				if ((item.iArchId == iGoodId) && (item.bMission != bMission))
				{
					RemoveCargo(player, item.iId, item.iCount);
					iCount += item.iCount;
				}
			}

			pub::Player::AddCargo(client, iGoodId, iCount, 1, bMission);
		}
		else
		{
			for (int i = 0; (i < iCount); i++)
				pub::Player::AddCargo(client, iGoodId, 1, 1, bMission);
		}

		if (iBase)
		{ 
			// player docked on base
			///////////////////////////////////////////////////
			// fix, else we get anti-cheat msg when undocking
			// this DOES NOT disable anti-cheat-detection, we're
			// just making some adjustments so that we dont get kicked

			Server.BaseEnter(iBase, client);
			if (iLocation)
				Server.LocationEnter(iLocation, client);
		}

		return {};
	}

	cpp::result<void, Error> AddCargo(const std::variant<uint, std::wstring>& player, const std::wstring& wscGood, int iCount, bool bMission)
	{
		uint iGoodId = ToInt(wscGood.c_str());
		if (!iGoodId)
			pub::GetGoodID(iGoodId, wstos(wscGood).c_str());
		if (!iGoodId)
			return cpp::fail(Error::InvalidGood);

		return AddCargo(player, iGoodId, iCount, bMission);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> Rename(const std::variant<uint, std::wstring>& player, const std::wstring& wscNewCharname, bool bOnlyDelete)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if ((client == -1) && player.index() && !Hk::Client::GetAccountByClientID(client))
			return cpp::fail(Error::CharacterDoesNotExist);

		if (!bOnlyDelete && Hk::Client::GetAccountByCharName(wscNewCharname))
			return cpp::fail(Error::AlreadyExists);

		if (!bOnlyDelete && (wscNewCharname.length() > 23))
			return cpp::fail(Error::CharacterNameTooLong);

		if (!bOnlyDelete && !wscNewCharname.length())
			return cpp::fail(Error::CharacterNameTooShort);

		INI_Reader ini;
		if (!bOnlyDelete && !(ini.open("..\\DATA\\CHARACTERS\\newcharacter.ini", false)))
			return cpp::fail(Error::MpNewCharacterFileNotFoundOrInvalid);

		CAccount* acc;
		std::wstring oldCharName;
		if (client != -1)
		{
			acc = Players.FindAccountFromClientID(client);
			oldCharName = (wchar_t*)Players.GetActiveCharacterName(client);
		}
		else
		{
			oldCharName = std::get<std::wstring>(player);
			acc = Hk::Client::GetAccountByCharName(std::get<std::wstring>(player)).value();
		}

		const std::wstring wscAccountDirname = Hk::Client::GetAccountDirName(acc);
		const auto newFileName = Hk::Client::GetCharFileName(wscNewCharname);
		if (newFileName.has_error())
		{
			return cpp::fail(newFileName.error());
		}

		const auto oldFileName = Hk::Client::GetCharFileName(oldCharName);
		if (oldFileName.has_error())
		{
			return cpp::fail(oldFileName.error());
		}

		std::string scNewCharfilePath = scAcctPath + wstos(wscAccountDirname) + "\\" + wstos(newFileName.value()) + ".fl";
		std::string scOldCharfilePath = scAcctPath + wstos(wscAccountDirname) + "\\" + wstos(oldFileName.value()) + ".fl";

		if (bOnlyDelete)
		{
			// delete character
			st6::wstring str((ushort*)oldCharName.c_str());
			Hk::Client::LockAccountAccess(acc, true); // also kicks player on this account
			Players.DeleteCharacterFromName(str);
			Hk::Client::UnlockAccountAccess(acc);
			return {};
		}

		Hk::Client::LockAccountAccess(acc, true); // kick player if online
		Hk::Client::UnlockAccountAccess(acc);

		// Copy existing char file into tmp
		std::string scTmpPath = scOldCharfilePath + ".tmp";
		DeleteFile(scTmpPath.c_str());
		CopyFile(scOldCharfilePath.c_str(), scTmpPath.c_str(), FALSE);

		// Delete existing char otherwise a rename of the char in slot 5 fails.
		st6::wstring str((ushort*)oldCharName.c_str());
		Players.DeleteCharacterFromName(str);

		// Emulate char create
		SLoginInfo logindata;
		wcsncpy_s(logindata.wszAccount, Hk::Client::GetAccountID(acc).value().c_str(), 36);
		Players.login(logindata, MaxClientId + 1);

		SCreateCharacterInfo newcharinfo;
		wcsncpy_s(newcharinfo.wszCharname, wscNewCharname.c_str(), 23);
		newcharinfo.wszCharname[23] = 0;

		newcharinfo.iNickName = 0;
		newcharinfo.iBase = 0;
		newcharinfo.iPackage = 0;
		newcharinfo.iPilot = 0;

		while (ini.read_header())
		{
			if (ini.is_header("Faction"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
						newcharinfo.iNickName = CreateID(ini.get_value_string());
					else if (ini.is_value("base"))
						newcharinfo.iBase = CreateID(ini.get_value_string());
					else if (ini.is_value("Package"))
						newcharinfo.iPackage = CreateID(ini.get_value_string());
					else if (ini.is_value("Pilot"))
						newcharinfo.iPilot = CreateID(ini.get_value_string());
				}
				break;
			}
		}
		ini.close();

		if (newcharinfo.iNickName == 0)
			newcharinfo.iNickName = CreateID("new_player");
		if (newcharinfo.iBase == 0)
			newcharinfo.iBase = CreateID("Li01_01_Base");
		if (newcharinfo.iPackage == 0)
			newcharinfo.iPackage = CreateID("ge_fighter");
		if (newcharinfo.iPilot == 0)
			newcharinfo.iPilot = CreateID("trent");

		// Fill struct with valid data (though it isnt used it is needed)
		newcharinfo.iDunno[4] = 65536;
		newcharinfo.iDunno[5] = 65538;
		newcharinfo.iDunno[6] = 0;
		newcharinfo.iDunno[7] = 1058642330;
		newcharinfo.iDunno[8] = 3206125978;
		newcharinfo.iDunno[9] = 65537;
		newcharinfo.iDunno[10] = 0;
		newcharinfo.iDunno[11] = 3206125978;
		newcharinfo.iDunno[12] = 65539;
		newcharinfo.iDunno[13] = 65540;
		newcharinfo.iDunno[14] = 65536;
		newcharinfo.iDunno[15] = 65538;
		Server.CreateNewCharacter(newcharinfo, MaxClientId + 1);
		SaveChar(wscNewCharname);
		Players.logout(MaxClientId + 1);

		// Decode the backup of the old char and overwrite the new char file
		if (!flc_decode(scTmpPath.c_str(), scNewCharfilePath.c_str()))
		{                                          // file wasn't encoded, thus
			                                       // simply rename it
			DeleteFile(scNewCharfilePath.c_str()); // just to get sure...
			CopyFile(scTmpPath.c_str(), scNewCharfilePath.c_str(), FALSE);
		}
		DeleteFile(scTmpPath.c_str());

		// Update the char name in the new char file.
		// Add a space to the value so the ini file line looks like "<key> =
		// <value>" otherwise Ioncross Server Operator can't decode the file
		// correctly
		std::string scValue = " ";
		for (uint i = 0; (i < wscNewCharname.length()); i++)
		{
			char cHiByte = wscNewCharname[i] >> 8;
			char cLoByte = wscNewCharname[i] & 0xFF;
			char szBuf[8];
			sprintf_s(szBuf, "%02X%02X", ((uint)cHiByte) & 0xFF, ((uint)cLoByte) & 0xFF);
			scValue += szBuf;
		}
		IniWrite(scNewCharfilePath, "Player", "Name", scValue);

		// Re-encode the char file if needed.
		if (!FLHookConfig::i()->general.disableCharfileEncryption)
			if (!flc_encode(scNewCharfilePath.c_str(), scNewCharfilePath.c_str()))
				return cpp::fail(Error::CouldNotEncodeCharFile);
	
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> MsgAndKick(ClientId client, const std::wstring& wscReason, uint iIntervall)
	{
		if (!ClientInfo[client].tmKickTime)
		{
			std::wstring wscMsg = ReplaceStr(FLHookConfig::i()->msgStyle.kickMsg, L"%reason", XMLText(wscReason));
			Hk::Message::FMsg(client, wscMsg);
			ClientInfo[client].tmKickTime = timeInMS() + iIntervall;
		}

		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> Kill(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		// check if logged in
		if (client == -1)
			return cpp::fail(Error::PlayerNotLoggedIn);

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
			return cpp::fail(Error::PlayerNotInSpace);

		pub::SpaceObj::SetRelativeHealth(ship, 0.0f);

		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<bool, Error> GetReservedSlot(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		CAccount* acc;
		if (client != -1)
			acc = Players.FindAccountFromClientID(client);
		else
			acc = player.index() ? Hk::Client::GetAccountByCharName(std::get<std::wstring>(player)).value() : nullptr;

		if (!acc)
			return cpp::fail(Error::CharacterDoesNotExist);

		const auto dir = Hk::Client::GetAccountDirName(acc);
		std::string scUserFile = scAcctPath + wstos(dir) + "\\flhookuser.ini";

		return IniGetB(scUserFile, "Settings", "ReservedSlot", false);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> SetReservedSlot(const std::variant<uint, std::wstring>& player, bool bReservedSlot)
	{
		auto acc = Hk::Client::ExtractAccount(player);
		if (acc.has_error())
			return cpp::fail(Error::CharacterDoesNotExist);

		const auto dir = Hk::Client::GetAccountDirName(acc.value());
		std::string scUserFile = scAcctPath + wstos(dir) + "\\flhookuser.ini";

		if (bReservedSlot)
			IniWrite(scUserFile, "Settings", "ReservedSlot", "yes");
		else
			IniWrite(scUserFile, "Settings", "ReservedSlot", "no");

		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct AUTOBUY_CARTITEM
	{
		uint iArchId;
		uint iCount;
		std::wstring wscDescription;
	};

	int PlayerAutoBuyGetCount(const std::list<CARGO_INFO>& lstCargo, uint iItemArchId)
	{
		for (auto const& cargo : lstCargo)
		{
			if (cargo.iArchId == iItemArchId)
				return cargo.iCount;
		}

		return 0;
	}

	void AddEquipToCart(const std::list<CARGO_INFO>& cargo, std::list<AUTOBUY_CARTITEM>& cart, AUTOBUY_CARTITEM item, const std::wstring desc) 
	{
		item.iCount = MAX_PLAYER_AMMO - PlayerAutoBuyGetCount(cargo, item.iArchId);
		item.wscDescription = desc;
		cart.emplace_back(item);
	}

	void PlayerAutoBuy(ClientId client, uint iBaseId)
	{
		// player cargo
		int iRemHoldSize;
		const auto cargo = Hk::Player::EnumCargo(client, iRemHoldSize);
		if ( cargo.has_error())
		{
			return;
		}

		// shopping cart
		std::list<AUTOBUY_CARTITEM> lstCart;

		if (ClientInfo[client].bAutoBuyReload)
		{ 
			// shield bats & nanobots
			Archetype::Ship* ship = Archetype::GetShip(Players[client].shipArchetype);

			uint iNanobotsId;
			pub::GetGoodID(iNanobotsId, "ge_s_repair_01");
			uint iRemNanobots = ship->iMaxNanobots;
			uint iShieldBatsId;
			pub::GetGoodID(iShieldBatsId, "ge_s_battery_01");
			uint iRemShieldBats = ship->iMaxShieldBats;
			bool bNanobotsFound = false;
			bool bShieldBattsFound = false;
			for (auto& item : cargo.value())
			{
				AUTOBUY_CARTITEM aci;
				if (item.iArchId == iNanobotsId)
				{
					aci.iArchId = iNanobotsId;
					aci.iCount = ship->iMaxNanobots - item.iCount;
					aci.wscDescription = L"Nanobots";
					lstCart.push_back(aci);
					bNanobotsFound = true;
				}
				else if (item.iArchId == iShieldBatsId)
				{
					aci.iArchId = iShieldBatsId;
					aci.iCount = ship->iMaxShieldBats - item.iCount;
					aci.wscDescription = L"Shield Batteries";
					lstCart.push_back(aci);
					bShieldBattsFound = true;
				}
			}

			if (!bNanobotsFound)
			{ // no nanos found -> add all
				AUTOBUY_CARTITEM aci;
				aci.iArchId = iNanobotsId;
				aci.iCount = ship->iMaxNanobots;
				aci.wscDescription = L"Nanobots";
				lstCart.push_back(aci);
			}

			if (!bShieldBattsFound)
			{ // no batts found -> add all
				AUTOBUY_CARTITEM aci;
				aci.iArchId = iShieldBatsId;
				aci.iCount = ship->iMaxShieldBats;
				aci.wscDescription = L"Shield Batteries";
				lstCart.push_back(aci);
			}
		}

		if (ClientInfo[client].bAutoBuyCD || ClientInfo[client].bAutoBuyCM || ClientInfo[client].bAutoBuyMines || ClientInfo[client].bAutoBuyMissiles ||
		    ClientInfo[client].bAutoBuyTorps)
		{
			// add mounted equip to a new list and eliminate double equipment(such
			// as 2x lancer etc)
			std::list<CARGO_INFO> lstMounted;
			for (auto& item : cargo.value())
			{
				if (!item.bMounted)
					continue;

				bool bFound = false;
				for (auto& mounted : lstMounted)
				{
					if (mounted.iArchId == item.iArchId)
					{
						bFound = true;
						break;
					}
				}

				if (!bFound)
					lstMounted.push_back(item);
			}

			uint iVFTableMines = (uint)hModCommon + ADDR_COMMON_VFTABLE_MINE;
			uint iVFTableCM = (uint)hModCommon + ADDR_COMMON_VFTABLE_CM;
			uint iVFTableGun = (uint)hModCommon + ADDR_COMMON_VFTABLE_GUN;

			// check mounted equip
			for (auto& mounted : lstMounted)
			{
				uint i = mounted.iArchId;
				AUTOBUY_CARTITEM aci;
				Archetype::Equipment* eq = Archetype::GetEquipment(mounted.iArchId);
				auto eqType = Hk::Client::GetEqType(eq);

				switch (eqType)
				{
					case ET_MINE: 
					{
						if (ClientInfo[client].bAutoBuyMines)
							AddEquipToCart(cargo.value(), lstCart, aci, L"Mines");

						break;
					}
					case ET_CM: 
					{
						if (ClientInfo[client].bAutoBuyCM)
							AddEquipToCart(cargo.value(), lstCart, aci, L"Countermeasures");

						break;
					}
					case ET_TORPEDO: 
					{
						if (ClientInfo[client].bAutoBuyTorps)
							AddEquipToCart(cargo.value(), lstCart, aci, L"Torpedoes");

						break;
					}
					case ET_CD: 
					{
						if (ClientInfo[client].bAutoBuyCD)
							AddEquipToCart(cargo.value(), lstCart, aci, L"Cruise Disrupters");

						break;
					}
					case ET_MISSILE: 
					{
						if (ClientInfo[client].bAutoBuyMissiles)
							AddEquipToCart(cargo.value(), lstCart, aci, L"Missiles");

						break;
					}

					default:
						break;
				}
			}
		}

		// search base in base-info list
		BASE_INFO* bi = 0;
		for (auto& base : lstBases)
		{
			if (base.baseId == iBaseId)
			{
				bi = &base;
				break;
			}
		}

		if (!bi)
			return; // base not found

		const auto cashErr = GetCash(client);
		if (cashErr.has_error())
		{
			return;
		}

		auto cash = cashErr.value();

		for (auto& buy : lstCart)
		{
			if (!buy.iCount || !Arch2Good(buy.iArchId))
				continue;

			// check if good is available and if player has the neccessary rep
			bool bGoodAvailable = false;
			for (auto& available : bi->lstMarketMisc)
			{
				if (available.iArchId == buy.iArchId)
				{
					// get base rep
					int iSolarRep;
					pub::SpaceObj::GetSolarRep(bi->iObjectId, iSolarRep);
					uint iBaseRep;
					pub::Reputation::GetAffiliation(iSolarRep, iBaseRep);
					if (iBaseRep == -1)
						continue; // rep can't be determined yet(space object not
						          // created yet?)

					// get player rep
					int iRepId;
					pub::Player::GetRep(client, iRepId);

					// check if rep is sufficient
					float fPlayerRep;
					pub::Reputation::GetGroupFeelingsTowards(iRepId, iBaseRep, fPlayerRep);
					if (fPlayerRep < available.fRep)
						break; // bad rep, not allowed to buy
					bGoodAvailable = true;
					break;
				}
			}

			if (!bGoodAvailable)
				continue; // base does not sell this item or bad rep

			float fPrice;
			if (pub::Market::GetPrice(iBaseId, buy.iArchId, fPrice) == -1)
				continue; // good not available

			Archetype::Equipment* eq = Archetype::GetEquipment(buy.iArchId);
			if (iRemHoldSize < (eq->fVolume * buy.iCount))
			{
				uint iNewCount = (uint)(iRemHoldSize / eq->fVolume);
				if (!iNewCount)
				{
					//				PrintUserCmdText(client,
					// L"Auto-Buy(%s): FAILED! Insufficient cargo space",
					// (*it4).wscDescription.c_str());
					continue;
				}
				else
					buy.iCount = iNewCount;
			}

			uint uCost = ((uint)fPrice * buy.iCount);
			if (cash < uCost)
				PrintUserCmdText(client, L"Auto-Buy(%s): FAILED! Insufficient Credits", buy.wscDescription.c_str());
			else
			{
				RemoveCash(client, uCost);
				cash -= uCost;
				iRemHoldSize -= ((int)eq->fVolume * buy.iCount);

				// add the item, dont use addcargo for performance/bug reasons
				// assume we only mount multicount goods (missiles, ammo, bots)
				pub::Player::AddCargo(client, buy.iArchId, buy.iCount, 1, false);

				PrintUserCmdText(client, L"Auto-Buy(%s): Bought %u unit(s), cost: %s$", buy.wscDescription.c_str(), buy.iCount, ToMoneyStr(uCost).c_str());
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> ResetRep(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		// check if logged in
		if (client == -1)
			return cpp::fail(Error::PlayerNotLoggedIn);

		INI_Reader ini;
		if (!ini.open("mpnewcharacter.fl", false))
			return cpp::fail(Error::MpNewCharacterFileNotFoundOrInvalid);

		ini.read_header();
		if (!ini.is_header("Player"))
		{
			ini.close();
			return cpp::fail(Error::MpNewCharacterFileNotFoundOrInvalid);
		}

		int iPlayerRep;
		pub::Player::GetRep(client, iPlayerRep);
		while (ini.read_value())
		{
			if (ini.is_value("house"))
			{
				float fRep = ini.get_value_float(0);
				const char* szRepGroupName = ini.get_value_string(1);

				uint iRepGroupId;
				pub::Reputation::GetReputationGroup(iRepGroupId, szRepGroupName);
				pub::Reputation::SetReputation(iPlayerRep, iRepGroupId, fRep);
			}
		}

		ini.close();
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> SetRep(const std::variant<uint, std::wstring>& player, const std::wstring& wscRepGroup, float fValue)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		// check if logged in
		if (client == -1)
			return cpp::fail(Error::PlayerNotLoggedIn);

		uint iRepGroupId;
		pub::Reputation::GetReputationGroup(iRepGroupId, wstos(wscRepGroup).c_str());
		if (iRepGroupId == -1)
			return cpp::fail(Error::InvalidRepGroup);

		int iPlayerRep;
		pub::Player::GetRep(client, iPlayerRep);
		pub::Reputation::SetReputation(iPlayerRep, iRepGroupId, fValue);
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<float, Error> GetRep(const std::variant<uint, std::wstring>& player, const std::wstring& wscRepGroup)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1)
			return cpp::fail(Error::PlayerNotLoggedIn);

		uint iRepGroupId;
		pub::Reputation::GetReputationGroup(iRepGroupId, wstos(wscRepGroup).c_str());
		if (iRepGroupId == -1)
			return cpp::fail(Error::InvalidRepGroup);

		int iPlayerRep;
		pub::Player::GetRep(client, iPlayerRep);
		float fValue;
		pub::Reputation::GetGroupFeelingsTowards(iPlayerRep, iRepGroupId, fValue);
		return fValue;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<std::vector<GROUP_MEMBER>, Error> GetGroupMembers(const std::variant<uint, std::wstring>& player)
	{
		std::vector<GROUP_MEMBER> members;
		ClientId client = Hk::Client::ExtractClientID(player);

		// check if logged in
		if (client == -1)
			return cpp::fail(Error::PlayerNotLoggedIn);

		// hey, at least it works! beware of the VC optimiser.
		st6::vector<uint> vMembers;
		pub::Player::GetGroupMembers(client, vMembers);

		for (uint i : vMembers)
		{
			GROUP_MEMBER gm = {i, (wchar_t*)Players.GetActiveCharacterName(i)};
			members.push_back(gm);
		}

		return members;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<std::list<std::wstring>, Error> ReadCharFile(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		std::wstring dir;
		cpp::result<CAccount*, Error> acc;
		if (client != -1)
		{
			acc = Players.FindAccountFromClientID(client);
			const wchar_t* wszCharname = (wchar_t*)Players.GetActiveCharacterName(client);
			if (!wszCharname)
				return cpp::fail(Error::CharacterNotSelected);

			dir = Hk::Client::GetAccountDirName(acc.value());
		}
		else
		{
			acc = Hk::Client::ExtractAccount(player);
			if (!acc)
				return cpp::fail(Error::CharacterDoesNotExist);
		}

		auto file = Hk::Client::GetCharFileName(player);
		if (file.has_error())
		{
			return cpp::fail(file.error());
		}

		std::string scCharFile = scAcctPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl";
		std::string scFileToRead;
		bool bDeleteAfter;
		if (Hk::Client::IsEncoded(scCharFile))
		{
			std::string scCharFileNew = scCharFile + ".ini";
			if (!flc_decode(scCharFile.c_str(), scCharFileNew.c_str()))
				return cpp::fail(Error::CouldNotDecodeCharFile);
			scFileToRead = scCharFileNew;
			bDeleteAfter = true;
		}
		else
		{
			scFileToRead = scCharFile;
			bDeleteAfter = false;
		}

		std::ifstream ifs;
		ifs.open(scFileToRead.c_str(), std::ios_base::in);
		if (!ifs.is_open())
			return cpp::fail(Error::UnknownError);

		std::list<std::string> output;
		std::string scLine;
		while (getline(ifs, scLine))
			output.emplace_back(scLine);
		ifs.close();
		if (bDeleteAfter)
			DeleteFile(scFileToRead.c_str());

		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> WriteCharFile(const std::variant<uint, std::wstring>& player, std::wstring wscData)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		
		if (client == -1)
		{
			return cpp::fail(Error::InvalidClientId);
		}
		
		const auto acc = Players.FindAccountFromClientID(client);
		const wchar_t* wszCharname = (wchar_t*)Players.GetActiveCharacterName(client);
		if (!wszCharname)
			return cpp::fail(Error::CharacterNotSelected);

		auto dir = Hk::Client::GetAccountDirName(acc);

		const auto file = Hk::Client::GetCharFileName(player);
		if (file.has_error())
		{
			return cpp::fail(file.error());
		}

		std::string scCharFile = scAcctPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl";
		std::string scFileToWrite;
		bool bEncode;
		if (Hk::Client::IsEncoded(scCharFile))
		{
			scFileToWrite = scCharFile + ".ini";
			bEncode = true;
		}
		else
		{
			scFileToWrite = scCharFile;
			bEncode = false;
		}

		std::ofstream ofs;
		ofs.open(scFileToWrite.c_str(), std::ios_base::out);
		if (!ofs.is_open())
			return cpp::fail(Error::UnknownError);

		size_t iPos;
		while ((iPos = wscData.find(L"\\n")) != -1)
		{
			std::wstring wscLine = wscData.substr(0, iPos);
			ofs << wstos(wscLine) << std::endl;
			wscData.erase(0, iPos + 2);
		}

		if (wscData.length())
			ofs << wstos(wscData);

		ofs.close();
		if (bEncode)
		{
			flc_encode(scFileToWrite.c_str(), scCharFile.c_str());
			DeleteFile(scFileToWrite.c_str());
		}
		return {};
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	cpp::result<void, Error> PlayerRecalculateCRC(ClientId client)
	{
		try
		{
			PlayerData* pd = &Players[client];
			char* ACCalcCRC = (char*)hModServer + 0x6FAF0;
			__asm {
			pushad
			mov ecx, [pd]
			call[ACCalcCRC]
			mov ecx, [pd]
			mov[ecx + 320h], eax
			popad
			}
		}
		catch (...)
		{
			return cpp::fail(Error::InvalidClientId);
		}

		return {};
	}

	/** Move the client to the specified location */
	void RelocateClient(ClientId client, Vector vDestination, Matrix mOrientation)
	{
		Quaternion qRotation = Math::MatrixToQuaternion(mOrientation);

		FLPACKET_LAUNCH pLaunch;
		pLaunch.ship = ClientInfo[client].ship;
		pLaunch.iBase = 0;
		pLaunch.iState = 0xFFFFFFFF;
		pLaunch.fRotate[0] = qRotation.w;
		pLaunch.fRotate[1] = qRotation.x;
		pLaunch.fRotate[2] = qRotation.y;
		pLaunch.fRotate[3] = qRotation.z;
		pLaunch.fPos[0] = vDestination.x;
		pLaunch.fPos[1] = vDestination.y;
		pLaunch.fPos[2] = vDestination.z;

		HookClient->Send_FLPACKET_SERVER_LAUNCH(client, pLaunch);

		uint iSystem;
		pub::Player::GetSystem(client, iSystem);
		pub::SpaceObj::Relocate(ClientInfo[client].ship, iSystem, vDestination, mOrientation);
	}

	/** Dock the client immediately */
	cpp::result<void, Error> InstantDock(ClientId client, uint iDockObj)
	{
		// check if logged in
		if (client == -1)
			return cpp::fail(Error::PlayerNotLoggedIn);

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
			return cpp::fail(Error::PlayerNotInSpace);

		uint iSystem;
		uint iSystem2;
		pub::SpaceObj::GetSystem(ship, iSystem);
		pub::SpaceObj::GetSystem(iDockObj, iSystem2);
		if (iSystem != iSystem2)
		{
			return cpp::fail(Error::PlayerNotInSpace);
		}

		try
		{
			pub::SpaceObj::InstantDock(ship, iDockObj, 1);
		}
		catch (...)
		{
			return cpp::fail(Error::PlayerNotInSpace);
		}

		return {};
	}

	cpp::result<int, Error> GetRank(const std::variant<uint, std::wstring>& player)
	{
		auto rank = Hk::Ini::GetFromPlayerFile(player, L"rank");
		if (rank.has_error())
		{
			return cpp::fail(rank.error());
		}

		return rank.value().length() ? ToInt(rank.value()) : 0;
	}

	/// Get online time.
	cpp::result<int, Error> GetOnlineTime(const std::variant<uint, std::wstring>& player)
	{
		const auto client = Hk::Client::ExtractClientID(player);
		const auto acc = Hk::Client::GetAccountByClientID(client);
		auto dir = Hk::Client::GetAccountDirName(acc);

		const auto file = Hk::Client::GetCharFileName(player);
		if (file.has_error())
		{
			return cpp::fail(file.error());
		}

		std::string scCharFile = scAcctPath + wstos(dir) + "\\" + wstos(file.value()) + ".fl";
		if (Hk::Client::IsEncoded(scCharFile))
		{
			std::string scCharFileNew = scCharFile + ".ini";
			if (!flc_decode(scCharFile.c_str(), scCharFileNew.c_str()))
				return cpp::fail(Error::CouldNotDecodeCharFile);

			int secs = IniGetI(scCharFileNew, "mPlayer", "total_time_played", 0);
			DeleteFile(scCharFileNew.c_str());
			return secs;
		}
		else
		{
			return IniGetI(scCharFile, "mPlayer", "total_time_played", 0);
		}
	}

	cpp::result<const uint, Error> GetSystemByNickname(std::variant<std::string, std::wstring> nickname)
	{
		uint system = 0;
		const std::string nick = nickname.index() == 0 ? std::get<std::string>(nickname) : wstos(std::get<std::wstring>(nickname));
		pub::GetSystemID(system, nick.c_str());
		if (!system)
			return cpp::fail(Error::InvalidSystem);

		return system;
	}

	CShip* CShipFromShipDestroyed(const DWORD** ecx)
	{
		return reinterpret_cast<CShip*>((*ecx)[4]); // NOLINT(performance-no-int-to-ptr)
	}

	/// Return true if this player is within the specified distance of any other player.
	bool IsInRange(ClientId client, float fDistance)
	{
		const auto lstMembers = GetGroupMembers((const wchar_t*)Players.GetActiveCharacterName(client));
		if (lstMembers.has_error())
		{
			return false;
		}

		uint ship;
		pub::Player::GetShip(client, ship);

		Vector pos;
		Matrix rot;
		pub::SpaceObj::GetLocation(ship, pos, rot);

		uint iSystem;
		pub::Player::GetSystem(client, iSystem);

		// For all players in system...
		struct PlayerData* playerDb = 0;
		while (playerDb = Players.traverse_active(playerDb))
		{
			// Get the this player's current system and location in the system.
			ClientId client2 = playerDb->iOnlineId;
			uint iSystem2 = 0;
			pub::Player::GetSystem(client2, iSystem2);
			if (iSystem != iSystem2)
				continue;

			uint ship2;
			pub::Player::GetShip(client2, ship2);

			Vector pos2;
			Matrix rot2;
			pub::SpaceObj::GetLocation(ship2, pos2, rot2);

			// Ignore players who are in your group.
			bool bGrouped = false;
			for (auto& gm : lstMembers.value())
			{
				if (gm.client == client2)
				{
					bGrouped = true;
					break;
				}
			}
			if (bGrouped)
				continue;

			// Is player within the specified range of the sending char.
			if (Hk::Math::Distance3D(pos, pos2) < fDistance)
				return true;
		}
		return false;
	}

	/**
	Delete a character.
	*/
	void DeleteCharacter(CAccount* acc, std::wstring& character)
	{
		Hk::Client::LockAccountAccess(acc, true);
		st6::wstring str((ushort*)character.c_str());
		Players.DeleteCharacterFromName(str);
		Hk::Client::UnlockAccountAccess(acc);
	}

	/**
	Create a new character in the specified account by emulating a
	create character.
	*/
	cpp::result<void, Error> NewCharacter(CAccount* acc, std::wstring& character)
	{
		Hk::Client::LockAccountAccess(acc, true);
		Hk::Client::UnlockAccountAccess(acc);

		INI_Reader ini;
		if (!ini.open("..\\DATA\\CHARACTERS\\newcharacter.ini", false))
			return cpp::fail(Error::MpNewCharacterFileNotFoundOrInvalid);

		// Emulate char create by logging in.
		SLoginInfo logindata;
		wcsncpy_s(logindata.wszAccount, Hk::Client::GetAccountID(acc).value().c_str(), 36);
		Players.login(logindata, Players.GetMaxPlayerCount() + 1);

		SCreateCharacterInfo newcharinfo;
		wcsncpy_s(newcharinfo.wszCharname, character.c_str(), 23);
		newcharinfo.wszCharname[23] = 0;

		newcharinfo.iNickName = 0;
		newcharinfo.iBase = 0;
		newcharinfo.iPackage = 0;
		newcharinfo.iPilot = 0;

		while (ini.read_header())
		{
			if (ini.is_header("Faction"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("nickname"))
						newcharinfo.iNickName = CreateID(ini.get_value_string());
					else if (ini.is_value("base"))
						newcharinfo.iBase = CreateID(ini.get_value_string());
					else if (ini.is_value("package"))
						newcharinfo.iPackage = CreateID(ini.get_value_string());
					else if (ini.is_value("pilot"))
						newcharinfo.iPilot = CreateID(ini.get_value_string());
				}
				break;
			}
		}
		ini.close();

		if (newcharinfo.iNickName == 0)
			newcharinfo.iNickName = CreateID("new_player");
		if (newcharinfo.iBase == 0)
			newcharinfo.iBase = CreateID("Li01_01_Base");
		if (newcharinfo.iPackage == 0)
			newcharinfo.iPackage = CreateID("ge_fighter");
		if (newcharinfo.iPilot == 0)
			newcharinfo.iPilot = CreateID("trent");

		// Fill struct with valid data (though it isnt used it is needed)
		newcharinfo.iDunno[4] = 65536;
		newcharinfo.iDunno[5] = 65538;
		newcharinfo.iDunno[6] = 0;
		newcharinfo.iDunno[7] = 1058642330;
		newcharinfo.iDunno[8] = 3206125978;
		newcharinfo.iDunno[9] = 65537;
		newcharinfo.iDunno[10] = 0;
		newcharinfo.iDunno[11] = 3206125978;
		newcharinfo.iDunno[12] = 65539;
		newcharinfo.iDunno[13] = 65540;
		newcharinfo.iDunno[14] = 65536;
		newcharinfo.iDunno[15] = 65538;
		Server.CreateNewCharacter(newcharinfo, Players.GetMaxPlayerCount() + 1);
		SaveChar(character);
		Players.logout(Players.GetMaxPlayerCount() + 1);

		return {};
	}

	/** Anti cheat checking code by mc_horst */
	cpp::result<void, Error> AntiCheat(ClientId client)
	{
		// check if ship in space
		uint ship = 0;
		pub::Player::GetShip(client, ship);
		if (ship)
			return {};

		char* szObjPtr;
		memcpy(&szObjPtr, &Players, 4);
		szObjPtr += 0x418 * (client - 1);

		char cRes;

		////////////////////////// 1
		__asm {
		mov ecx, [szObjPtr]
		call [AntiCheat1]
		mov [cRes], al
		}

		if (cRes != 0)
		{ // kick
			Kick(client);
			return cpp::fail(Error::UnknownError);
		}

		////////////////////////// 2
		__asm {
		mov ecx, [szObjPtr]
		call [AntiCheat2]
		mov [cRes], al
		}

		if (cRes != 0)
		{ // kick
			Kick(client);
			return cpp::fail(Error::UnknownError);
		}

		////////////////////////// 3
		ulong lRet;
		ulong lCompare;
		__asm {
		mov ecx, [szObjPtr]
		mov eax, [ecx+0x320]
		mov [lCompare], eax
		call [AntiCheat3]
		mov [lRet], eax
		}

		if (lRet > lCompare)
		{ // kick
			Kick(client);
			return cpp::fail(Error::UnknownError);
		}

		////////////////////////// 4
		__asm {
		mov ecx, [szObjPtr]
		call [AntiCheat4]
		mov [cRes], al
		}

		if (cRes != 0)
		{ // kick
			Kick(client);
			return cpp::fail(Error::UnknownError);
		}
	}

	cpp::result<void, Error> SetEquip(const std::variant<uint, std::wstring>& player, const st6::list<EquipDesc>& equip)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if ((client == -1) || Hk::Client::IsInCharSelectMenu(client))
			return cpp::fail(Error::CharacterNotSelected);

		// Update FLHook's lists to make anticheat pleased.
		if (&equip != &Players[client].lShadowEquipDescList.equip)
			Players[client].lShadowEquipDescList.equip = equip;

		if (&equip != &Players[client].equipDescList.equip)
			Players[client].equipDescList.equip = equip;

		// Calculate packet size. First two bytes reserved for items count.
		uint itemBufSize = 2;
		for (const auto& item : equip)
		{
			itemBufSize += sizeof(SetEquipmentItem) + strlen(item.szHardPoint.value) + 1;
		}

		FLPACKET* packet = FLPACKET::Create(itemBufSize, FLPACKET::FLPACKET_SERVER_SETEQUIPMENT);
		FlPacketSetEquipment* pSetEquipment = reinterpret_cast<FlPacketSetEquipment*>(packet->content);

		// Add items to packet as array of variable size.
		uint index = 0;
		for (const auto& item : equip)
		{
			SetEquipmentItem setEquipItem;
			setEquipItem.iCount = item.iCount;
			setEquipItem.fHealth = item.fHealth;
			setEquipItem.iArchId = item.iArchId;
			setEquipItem.sId = item.sId;
			setEquipItem.bMounted = item.bMounted;
			setEquipItem.bMission = item.bMission;

			if (uint len = strlen(item.szHardPoint.value); len && item.szHardPoint.value != "BAY")
			{
				setEquipItem.szHardPointLen = len + 1; // add 1 for the null - char* is a null-terminated string in C++
			}
			else
			{
				setEquipItem.szHardPointLen = 0;
			}
			pSetEquipment->count++;

			byte* buf = (byte*)&setEquipItem;
			for (int i = 0; i < sizeof(SetEquipmentItem); i++)
				pSetEquipment->items[index++] = buf[i];

			byte* szHardPoint = (byte*)item.szHardPoint.value;
			for (int i = 0; i < setEquipItem.szHardPointLen; i++)
				pSetEquipment->items[index++] = szHardPoint[i];
		}

		if (packet->SendTo(client))
		{
			return {};
		}
		return cpp::fail(Error::UnknownError);
	}

	cpp::result<void, Error> AddEquip(const std::variant<uint, std::wstring>& player, uint iGoodId, const std::string& scHardpoint)
	{
		ClientId client = Hk::Client::ExtractClientID(player);

		if ((client == -1) || Hk::Client::IsInCharSelectMenu(client))
			return cpp::fail(Error::CharacterNotSelected);

		if (!Players[client].iEnteredBase)
		{
			Players[client].iEnteredBase = Players[client].baseId;
			Server.ReqAddItem(iGoodId, scHardpoint.c_str(), 1, 1.0f, true, client);
			Players[client].iEnteredBase = 0;
		}
		else
		{
			Server.ReqAddItem(iGoodId, scHardpoint.c_str(), 1, 1.0f, true, client);
		}

		// Add to check-list which is being compared to the users equip-list when
		// saving char to fix "Ship or Equipment not sold on base" kick
		EquipDesc ed;
		ed.sId = Players[client].sLastEquipId;
		ed.iCount = 1;
		ed.iArchId = iGoodId;
		Players[client].lShadowEquipDescList.add_equipment_item(ed, false);

		return {};
	}

	cpp::result<void, Error> AddEquip(const std::variant<uint, std::wstring>& player, uint iGoodId, const std::string& scHardpoint, bool bMounted)
	{
		typedef bool(__stdcall * _AddCargoDocked)(uint iGoodId, CacheString * &hardpoint, int iNumItems, float fHealth, int bMounted, int bMission, uint iOne);
		static _AddCargoDocked AddCargoDocked = 0;
		if (!AddCargoDocked)
			AddCargoDocked = (_AddCargoDocked)((char*)hModServer + 0x6EFC0);

		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1 || Hk::Client::IsInCharSelectMenu(client))
			return cpp::fail(Error::PlayerNotLoggedIn);

		uint iBase = 0;
		pub::Player::GetBase(client, iBase);
		uint iLocation = 0;
		pub::Player::GetLocation(client, iLocation);

		if (iLocation)
			Server.LocationExit(iLocation, client);
		if (iBase)
			Server.BaseExit(iBase, client);
		if (!Hk::Client::IsValidClientID(client))
			return cpp::fail(Error::PlayerNotLoggedIn);

		PlayerData* pd = &Players[client];
		const char* p = scHardpoint.c_str();
		CacheString hardpoint;
		hardpoint.value = StringAlloc(p, false);

		int iOne = 1;
		int iMounted = bMounted;
		float fHealth = 1;
		CacheString* pHP = &hardpoint;
		__asm {
		push iOne
		push iMounted
		push iOne
		push fHealth
		push iOne
		push pHP
		push iGoodId
		mov ecx, pd
		call AddCargoDocked
		}

		if (iBase) Server.BaseEnter(iBase, client);
		if (iLocation)
			Server.LocationEnter(iLocation, client);

		return {};
	}

	std::wstring GetLocation(unsigned int client)
	{
		uint iSystemId = 0;
		uint ship = 0;
		pub::Player::GetSystem(client, iSystemId);
		pub::Player::GetShip(client, ship);
		if (!iSystemId || !ship)
		{
			PrintUserCmdText(client, L"ERR Not in space");
			return false;
		}

		Vector pos;
		Matrix rot;
		pub::SpaceObj::GetLocation(ship, pos, rot);

		return Hk::Math::VectorToSectorCoord<std::wstring>(iSystemId, pos);
	}

	void DelayedKick(ClientId client, uint secs)
	{
		mstime kick_time = timeInMS() + (secs * 1000);
		if (!ClientInfo[client].tmKickTime || ClientInfo[client].tmKickTime > kick_time)
			ClientInfo[client].tmKickTime = kick_time;
	}

	std::string GetPlayerSystemS(ClientId client)
	{
		uint iSystemId;
		pub::Player::GetSystem(client, iSystemId);
		char szSystemname[1024] = "";
		pub::GetSystemNickname(szSystemname, sizeof(szSystemname), iSystemId);
		return szSystemname;
	}

	cpp::result<const uint, Error> GetShipValue(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client != -1 && !Hk::Client::IsInCharSelectMenu(client))
		{
			SaveChar(player);
			if (!Hk::Client::IsValidClientID(client))
			{
				return cpp::fail(Error::UnknownError);
			}
		}

		float fValue = 0.0f;

		uint iBaseId = 0;

		const auto lstCharFile = ReadCharFile(player);
		if (lstCharFile.has_error())
		{
			return cpp::fail(lstCharFile.error());
		}

		for (const auto& line : lstCharFile.value())
		{
			std::wstring wscKey = Trim(line.substr(0, line.find(L"=")));
			if (wscKey == L"base" || wscKey == L"last_base")
			{
				int iFindEqual = line.find(L"=");
				if (iFindEqual == -1)
				{
					continue;
				}

				if ((iFindEqual + 1) >= (int)line.size())
				{
					continue;
				}

				iBaseId = CreateID(wstos(Trim(line.substr(iFindEqual + 1))).c_str());
				break;
			}
		}

		for (const auto& line : lstCharFile.value())
		{
			std::wstring wscKey = Trim(line.substr(0, line.find(L"=")));
			if (wscKey == L"cargo" || wscKey == L"equip")
			{
				int iFindEqual = line.find(L"=");
				if (iFindEqual == -1)
				{
					continue;
				}
				int iFindComma = line.find(L",", iFindEqual);
				if (iFindComma == -1)
				{
					continue;
				}
				uint iGoodId = ToUInt(Trim(line.substr(iFindEqual + 1, iFindComma)));
				uint iGoodCount = ToUInt(Trim(line.substr(iFindComma + 1, line.find(L",", iFindComma + 1))));

				float fItemValue;
				if (pub::Market::GetPrice(iBaseId, Arch2Good(iGoodId), fItemValue) == 0)
				{
					if (arch_is_combinable(iGoodId))
					{
						fValue += fItemValue * static_cast<float>(iGoodCount);
					}
					else
					{
						float const* fResaleFactor = (float*)((char*)hModServer + 0x8AE7C);
						fValue += fItemValue * (*fResaleFactor);
					}
				}
			}
			else if (wscKey == L"money")
			{
				int iFindEqual = line.find(L"=");
				if (iFindEqual == -1)
				{
					continue;
				}
				uint fItemValue = ToUInt(Trim(line.substr(iFindEqual + 1)));
				fValue += fItemValue;
			}
			else if (wscKey == L"ship_archetype")
			{
				uint shipArchId = ToUInt(Trim(line.substr(line.find(L"=") + 1, line.length())));
				const GoodInfo* gi = GoodList_get()->find_by_ship_arch(shipArchId);
				if (gi)
				{
					gi = GoodList::find_by_id(gi->iArchId);
					if (gi)
					{
						float* fResaleFactor = (float*)((char*)hModServer + 0x8AE78);
						float fItemValue = gi->fPrice * (*fResaleFactor);
						fValue += fItemValue;
					}
				}
			}
		}

		return static_cast<uint>(fValue);
	}

	void SaveChar(ClientId client)
	{
		BYTE patch[] = {0x90, 0x90};
		WriteProcMem((char*)hModServer + 0x7EFA8, patch, sizeof(patch));
		pub::Save(client, 1);
	}

	cpp::result<const ShipId, Error> GetTarget(const std::variant<uint, std::wstring>& player)
	{
		const auto ship = GetShip(player);
		if (ship.has_error())
			return cpp::fail(ship.error());

		uint target = Hk::Player::GetTarget(ship.value()).value();
		if (!ship)
			return cpp::fail(Error::NoTargetSelected);

		return target;
	}

	cpp::result<ClientId, Error> GetTargetClientID(const std::variant<uint, std::wstring>& player)
	{
		const auto target = GetTarget(player);
		if (target.has_error())
			return cpp::fail(target.error());

		auto targetClientId = Hk::Client::GetClientIdByShip(target.value());
		if (targetClientId.has_error())
			return cpp::fail(Error::TargetIsNotPlayer);

		return targetClientId;
	}

	cpp::result<const BaseId, Error> GetCurrentBase(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1)
		{
			return cpp::fail(Error::PlayerNotLoggedIn);
		}

		uint base;
		pub::Player::GetBase(client, base);
		if (base)
		{
			return base;
		}

		return cpp::fail(Error::PlayerNotDocked);
	}

	cpp::result<const SystemId, Error> GetSystem(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1)
		{
			return cpp::fail(Error::PlayerNotLoggedIn);
		}

		uint system;
		pub::Player::GetSystem(client, system);
		if (!system)
			return cpp::fail(Error::InvalidSystem);

		return system;
	}

	// returns ship instance ID
	cpp::result<const ShipId, Error> GetShip(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1)
		{
			return cpp::fail(Error::PlayerNotLoggedIn);
		}

		uint ship;
		pub::Player::GetShip(client, ship);
		if (!ship)
			return cpp::fail(Error::PlayerNotInSpace);

		return ship;
	}

	// returns Ship Type
	cpp::result<const uint, Error> GetShipID(const std::variant<uint, std::wstring>& player)
	{
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1)
		{
			return cpp::fail(Error::PlayerNotLoggedIn);
		}

		uint shipId;
		pub::Player::GetShipID(client, shipId);
		if (!shipId)
			return cpp::fail(Error::PlayerNotInSpace);

		return shipId;
	}

	cpp::result<void, Error> MarkObj(const std::variant<uint, std::wstring>& player, uint objId, int markStatus) {
		ClientId client = Hk::Client::ExtractClientID(player);
		if (client == -1)
		{
			return cpp::fail(Error::PlayerNotLoggedIn);
		}
		pub::Player::MarkObj(client, objId, markStatus);

		return {};
	}
} // namespace Hk::Player