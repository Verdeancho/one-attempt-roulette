#include "Common.hpp"
#include <Geode/ui/MDTextArea.hpp>

// ---------------------------------------------------------------------------
// "How to use": breve tutorial por paginas + creditos
// ---------------------------------------------------------------------------

static char const* const HOW_TO_PAGES[] = {
    // 1. Que es
    "# What is One Attempt Roulette?\n"
    "A level roulette where you only get <cy>one attempt</c> per level.\n\n"
    "- A random level appears and you play it **once**.\n"
    "- When you die you leave the level automatically and your percentage is **added to the total**.\n"
    "- Then the next random level loads, until you finish all the levels you chose.\n\n"
    "Try to get the highest total possible and beat your records!",

    // 2. Elegir ruleta
    "# Choosing a roulette\n"
    "Open it from the <cg>Roulette</c> button in the **Create** menu.\n\n"
    "- **Difficulty**: any rated level from Easy to Extreme Demon. Check *Force popular levels* and set a minimum number of downloads (e.g. 100000) to only get popular levels.\n"
    "- **Demonlist**: levels from Pointercrate. Use *Main List*, *Extended*, *Legacy* or type any range (e.g. 100 - 500).\n"
    "- **Specials**: recent levels, random levels from all of GD, friends' levels, gamemode challenges (wave, ship, ball...) and *The Challenge List*.\n\n"
    "Choose **Classic**, **Platformer** or **Both**, set how many levels you want and press **Start**.",

    // 3. Jugar
    "# Playing\n"
    "- Each level opens its normal info screen: set **Low Detail**, download the song, etc., and press play when you are ready.\n"
    "- Practice mode and restarting are **disabled**: you only have 1 attempt.\n"
    "- Quitting from the pause menu **counts as your attempt** (with the percentage you had).\n"
    "- Your roulette is **saved automatically**: you can leave and press **Continue** later.\n"
    "- **Abandon** saves the roulette in the history as abandoned.",

    // 4. Overlay y panel
    "# Overlay & panel\n"
    "- During the level, an **overlay** shows your progress, total, mean and standard deviation. With *Live total/mean* they update while you play.\n"
    "- On the level screen there is a **panel** with the stats of the roulette. Use the <cy>eye</c> to collapse it and the <cy>gear</c> to edit it.\n"
    "- In the editor (eye button in the roulette menu, or *Overlay* in the pause menu) you can **drag** them with the mouse and change size, opacity, background and the **colors** of each value.\n"
    "- In the *Notifications* tab of the editor you can move the notifications and turn each type on or off.\n"
    "- For streamers: enable *OBS text files* in the mod settings.",

    // 5. Estadisticas
    "# Statistics\n"
    "Press the stats button in the roulette menu.\n\n"
    "- Records for each category and number of levels: **best total**, **worst total** and **most 100%**.\n"
    "- **View** shows the history with dates. Open the graph of any roulette, and **View levels** to see every level and its percentage (click one to go to it).\n"
    "- Use the <cy>lock</c> to protect a roulette. The <cr>trash</c> deletes one roulette, or many at once while keeping your records.",
};
static constexpr int HOW_TO_PAGE_COUNT = sizeof(HOW_TO_PAGES) / sizeof(HOW_TO_PAGES[0]);

class HowToPopup : public Popup {
protected:
    static constexpr float W = 400.f;
    static constexpr float H = 280.f;

    int m_page = 0;
    CCNode* m_pageNode = nullptr;
    CCMenu* m_pageMenu = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;

    bool init() override {
        if (!Popup::init(W, H)) return false;
        this->setTitle("How to use", "goldFont.fnt", 0.75f, 18.f);

        m_pageNode = CCNode::create();
        m_pageNode->setContentSize({ W, H });
        m_mainLayer->addChildAtPosition(m_pageNode, Anchor::BottomLeft, { 0.f, 0.f });

        m_pageMenu = makeMenu({ W, H });
        m_mainLayer->addChildAtPosition(m_pageMenu, Anchor::Center);

        auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        prevSpr->setScale(0.6f);
        m_prevBtn = CCMenuItemExt::createSpriteExtra(prevSpr, [this](auto) { this->setPage(m_page - 1); });
        m_buttonMenu->addChildAtPosition(m_prevBtn, Anchor::Left, { -2.f, 0.f });

        auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        nextSpr->setScale(0.6f);
        nextSpr->setFlipX(true);
        m_nextBtn = CCMenuItemExt::createSpriteExtra(nextSpr, [this](auto) { this->setPage(m_page + 1); });
        m_buttonMenu->addChildAtPosition(m_nextBtn, Anchor::Right, { 2.f, 0.f });

        m_pageLabel = makeLabel("", "goldFont.fnt", 0.45f);
        m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::BottomLeft, { W / 2, 14.f });

        this->setPage(0);
        return true;
    }

    // la ultima pagina son los creditos
    int pageCount() const { return HOW_TO_PAGE_COUNT + 1; }

    void setPage(int page) {
        m_page = std::clamp(page, 0, this->pageCount() - 1);
        m_pageNode->removeAllChildren();
        m_pageMenu->removeAllChildren();

        if (m_page < HOW_TO_PAGE_COUNT) {
            auto text = MDTextArea::create(HOW_TO_PAGES[m_page], { W - 50.f, H - 70.f });
            text->ignoreAnchorPointForPosition(false);
            text->setAnchorPoint({ 0.5f, 0.5f });
            text->setPosition({ W / 2, H / 2 - 8.f });
            m_pageNode->addChild(text);
        } else {
            this->buildCredits();
        }

        m_pageLabel->setString(fmt::format("{}/{}", m_page + 1, this->pageCount()).c_str());
        m_prevBtn->setVisible(m_page > 0);
        m_nextBtn->setVisible(m_page < this->pageCount() - 1);
        handleTouchPriority(this);
    }

    // Avatar + nombre + boton para abrir un enlace
    void addCredit(float y, char const* avatar, char const* title, char const* name, char const* button, char const* url, char const* confirm) {
        float x = 70.f;
        if (auto spr = CCSprite::create(avatar)) {
            fitNode(spr, 40.f);
            spr->setPosition({ x, y });
            m_pageNode->addChild(spr);
        }
        auto t = makeLabel(title, "goldFont.fnt", 0.5f, { 0.f, 0.5f });
        t->setPosition({ x + 30.f, y + 9.f });
        m_pageNode->addChild(t);
        auto n = makeLabel(name, "bigFont.fnt", 0.45f, { 0.f, 0.5f });
        n->setPosition({ x + 30.f, y - 9.f });
        m_pageNode->addChild(n);

        std::string link = url;
        std::string question = confirm;
        auto btn = textButton(button, "GJ_button_06.png", 0.5f, [link, question](auto) {
            createQuickPopup("Open link", question, "Cancel", "Open", [link](auto, bool yes) {
                if (yes) web::openLinkInBrowser(link);
            });
        });
        btn->setPosition({ W - 80.f, y });
        m_pageMenu->addChild(btn);
    }

    void buildCredits() {
        auto heading = makeLabel("Credits", "bigFont.fnt", 0.6f);
        heading->setPosition({ W / 2, H - 58.f });
        m_pageNode->addChild(heading);

        this->addCredit(
            165.f, "guillester.png"_spr, "How to play video", "Sr.Guillester", "Watch video",
            HOW_TO_VIDEO_URL, "Open <cy>Sr.Guillester</c>'s video about the mod in your browser?"
        );
        this->addCredit(
            100.f, "jenrai.png"_spr, "Special thanks to", "Jenrai", "YouTube channel",
            JENRAI_URL, "Open <cy>Jenrai</c>'s YouTube channel in your browser?"
        );

        auto thanks = makeLabel("Thanks for playing!", "goldFont.fnt", 0.5f);
        thanks->setPosition({ W / 2, 48.f });
        m_pageNode->addChild(thanks);
    }

public:
    static HowToPopup* create() {
        auto ret = new HowToPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

void openHowToPopup() {
    if (auto popup = HowToPopup::create()) popup->show();
}
