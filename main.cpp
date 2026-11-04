#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

static bool bufferEnabled() {
	return Mod::get()->getSettingValue<bool>("enabled");
}

static int bufferTicks() {
	return static_cast<int>(Mod::get()->getSettingValue<int64_t>("buffer-ticks"));
}

// ---------------------------------------------------------------------------
// Buffering logic
// ---------------------------------------------------------------------------
class $modify(BufferGameLayer, GJBaseGameLayer) {
	struct Fields {
		int  ticksLeft[2] = {0, 0};     // remaining buffer ticks (P1, P2)
		bool held[2]      = {false, false};
		bool releaseNext[2] = {false, false};
	};

	PlayerObject* playerFor(int i) {
		return i == 0 ? m_player1 : m_player2;
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		GJBaseGameLayer::handleButton(down, button, isPlayer1);

		if (!bufferEnabled() || button != 1) return;

		int i = isPlayer1 ? 0 : 1;
		auto& f = *m_fields.self();
		f.held[i] = down;

		if (down) {
			auto* p = playerFor(i);
			// If we're already grounded the normal click worked, nothing to buffer.
			if (p && !p->m_isOnGround) {
				f.ticksLeft[i] = bufferTicks();
			} else {
				f.ticksLeft[i] = 0;
			}
		}
	}

	void processCommands(float dt) {
		auto& f = *m_fields.self();

		if (bufferEnabled()) {
			for (int i = 0; i < 2; i++) {
				if (f.ticksLeft[i] <= 0) continue;
				f.ticksLeft[i]--;

				auto* p = playerFor(i);
				if (p && p->m_isOnGround) {
					p->pushButton(PlayerButton::Jump);
					f.ticksLeft[i] = 0;
					// Player already let go of the button -> release right after the jump registers.
					if (!f.held[i]) f.releaseNext[i] = true;
				}
			}
		} else {
			f.ticksLeft[0] = f.ticksLeft[1] = 0;
		}

		GJBaseGameLayer::processCommands(dt);

		for (int i = 0; i < 2; i++) {
			if (f.releaseNext[i]) {
				f.releaseNext[i] = false;
				if (auto* p = playerFor(i)) p->releaseButton(PlayerButton::Jump);
			}
		}
	}
};

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------
class BufferPopup : public geode::Popup<> {
protected:
	CCLabelBMFont* m_valueLabel = nullptr;
	CCMenuItemToggler* m_toggle = nullptr;

	bool setup() override {
		this->setTitle("Input Buffer");

		auto size = m_mainLayer->getContentSize();
		auto menu = CCMenu::create();
		menu->setPosition({0, 0});
		m_mainLayer->addChild(menu);

		// Enable toggle
		m_toggle = CCMenuItemToggler::createWithStandardSprites(
			this, menu_selector(BufferPopup::onToggle), 0.8f
		);
		m_toggle->toggle(bufferEnabled());
		m_toggle->setPosition({size.width / 2 - 50, size.height - 60});
		menu->addChild(m_toggle);

		auto enLabel = CCLabelBMFont::create("Enabled", "bigFont.fnt");
		enLabel->setScale(0.5f);
		enLabel->setAnchorPoint({0, 0.5f});
		enLabel->setPosition({size.width / 2 - 30, size.height - 60});
		m_mainLayer->addChild(enLabel);

		// Tick label
		auto desc = CCLabelBMFont::create("Buffer (ticks)", "goldFont.fnt");
		desc->setScale(0.7f);
		desc->setPosition({size.width / 2, size.height - 95});
		m_mainLayer->addChild(desc);

		m_valueLabel = CCLabelBMFont::create(std::to_string(bufferTicks()).c_str(), "bigFont.fnt");
		m_valueLabel->setPosition({size.width / 2, size.height / 2 - 20});
		m_mainLayer->addChild(m_valueLabel);

		// Adjust buttons: -5 -1 +1 +5
		struct Btn { const char* text; int delta; float x; };
		Btn btns[] = {
			{"-5", -5, size.width / 2 - 100},
			{"-1", -1, size.width / 2 - 55},
			{"+1",  1, size.width / 2 + 55},
			{"+5",  5, size.width / 2 + 100},
		};
		for (auto& b : btns) {
			auto spr = ButtonSprite::create(b.text, 30, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f);
			auto item = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BufferPopup::onAdjust));
			item->setTag(b.delta);
			item->setPosition({b.x, size.height / 2 - 20});
			menu->addChild(item);
		}

		auto hint = CCLabelBMFont::create("240 ticks = 1 second at default TPS", "chatFont.fnt");
		hint->setScale(0.6f);
		hint->setOpacity(160);
		hint->setPosition({size.width / 2, 25});
		m_mainLayer->addChild(hint);

		return true;
	}

	void onToggle(CCObject*) {
		// toggler flips after the callback, so invert
		Mod::get()->setSettingValue<bool>("enabled", !m_toggle->isToggled());
	}

	void onAdjust(CCObject* sender) {
		int delta = static_cast<CCNode*>(sender)->getTag();
		int v = std::clamp(bufferTicks() + delta, 1, 60);
		Mod::get()->setSettingValue<int64_t>("buffer-ticks", v);
		m_valueLabel->setString(std::to_string(v).c_str());
	}

public:
	static BufferPopup* create() {
		auto ret = new BufferPopup();
		if (ret->initAnchored(300.f, 190.f)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

class $modify(BufferPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto winSize = CCDirector::get()->getWinSize();
		auto menu = CCMenu::create();
		menu->setPosition({0, 0});

		auto spr = ButtonSprite::create("Buffer", 60, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.6f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BufferPauseLayer::onBufferMenu));
		btn->setPosition({winSize.width - 50, 25});
		menu->addChild(btn);

		this->addChild(menu);
	}

	void onBufferMenu(CCObject*) {
		BufferPopup::create()->show();
	}
};
