﻿#pragma once

#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::Autobuy
{
	//! A struct to represent each client
	class AutobuyInfo
	{
	  public:
		AutobuyInfo() = default;

		bool missiles;
		bool mines;
		bool torps;
		bool cd;
		bool cm;
		bool bb;
		bool repairs;
	};

	struct AutobuyCartItem
	{
		uint archId = 0;
		uint count = 0;
		std::wstring description;
	};

	//! Configurable fields for this plugin
	struct Config final : Reflectable
	{
		std::string File() override { return "config/autobuy.json"; }

		// Reflectable fields
		//! Nickname of the nanobot item being used when performing the automatic purchase
		std::string nanobot_nickname = "ge_s_repair_01";
		//! Nickname of the shield battery item being used when performing the automatic purchase
		std::string shield_battery_nickname = "ge_s_battery_01";
	};

	struct Global final
	{
		std::unique_ptr<Config> config = nullptr;
		std::map<uint, AutobuyInfo> autobuyInfo;
		ReturnCode returnCode = ReturnCode::Default;
	};

} // namespace Plugins::Autobuy