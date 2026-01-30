#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(MyPlayer, PlayerObject) {
    int m_frameCounter = 0;

    void update(float dt) {
        PlayerObject::update(dt);

        // Чтобы призраки не плодились в меню
        if (!this->getParent()) return;

        auto spawnRate = Mod::get()->getSettingValue<int64_t>("spawn-rate");
        m_fields->m_frameCounter++;

        if (m_fields->m_frameCounter >= spawnRate) {
            m_fields->m_frameCounter = 0;
            this->createGhost();
        }
    }

    void createGhost() {
        // Получаем текущий спрайт игрока (учитываем иконку/робот/паук)
        // Для простоты берем основной контейнер иконки
        auto ghost = CCSprite::createWithSpriteFrame(this->displaySprite()->displayFrame());
        
        ghost->setPosition(this->getPosition());
        ghost->setRotation(this->getRotation());
        ghost->setScale(this->getScale());
        ghost->setFlipX(this->isFacingLeft());
        
        // Настройки из конфига
        auto initialOpacity = static_cast<GLubyte>(Mod::get()->getSettingValue<int64_t>("trail-opacity"));
        ghost->setOpacity(initialOpacity);
        
        // Смешивание (Additive blending) для эффекта свечения
        ghost->setBlendFunc({ GL_SRC_ALPHA, GL_ONE });

        // Добавляем на слой, где находится игрок
        this->getParent()->addChild(ghost, this->getZOrder() - 1);

        // Анимация затухания и удаления
        auto fadeOut = CCFadeOut::create(0.5f);
        auto remove = CCRemoveSelf::create();
        ghost->runAction(CCSequence::create(fadeOut, remove, nullptr));
    }
};
