#include "shadowowner.hh"

void ShadowOwner::setShadow(QQuickItem *shadow) {
    if (shadow == shadow_)
        return;
    shadow_ = shadow;
    // Its `material` is the effect it made itself until one is given it.
    if (shadow)
        if (auto *effect = shadow->property("material").value<QQuickItem *>(); effect && !effect->parent())
            effect->setParent(shadow);
    emit shadowChanged();
}
