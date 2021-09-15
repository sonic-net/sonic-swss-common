#ifndef __IFCOMMON_H__
#define __IFCOMMON_H__

namespace swss {
	class subIntf
	{
		public:
			subIntf(const std::string &ifName);
			bool isValid();
			std::string parentIntf();
			int subIntfIdx();
			std::string longName();
			std::string shortName();
			bool isShortName();

        private:
#define VLAN_SUB_INTERFACE_SEPARATOR   "."
            std::string alias = "";
			std::string subIfIdx = "";
			std::string parentIf = "";
			std::string parentIfShortName = "";
			bool isCompressed = false;
	};
}

#endif
