#pragma once
#include "precompiled.h"
#include "qdebug.h"

class cfg1
{
public:
	static float getOpt(string name, float defaultVal, std::function<bool()> shouldUpdate, std::function<float()> getVal) {
		if(opts.find(name) == opts.end()) {
			Opt opt1;
			opt1.name=name;
			opt1.shouldUpdate=shouldUpdate;
			opt1.getVal=getVal;
			opt1.val=defaultVal;
			opts[name]=opt1;
		}
		auto& opt = opts[name];
		if(opt.shouldUpdate()) {
			opt.val=opt.getVal();
		}
		return opt.val;
	}
	static void print() {
		qDebug() << "============ CFG values ============";
		foreach(auto& pair, opts) {
			auto& key=pair.first;
			auto opt = opts[key];
			qDebug() << opt.name << " = " << opt.val;
		}
	}
private:
	struct Opt
	{
		string name;
		std::function<float()> getVal;
		float val;
		std::function<bool()> shouldUpdate;
	};
	static std::map<string,Opt> opts;
};