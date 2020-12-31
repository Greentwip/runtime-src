-- Copyright 2014-2015 Greentwip. All Rights Reserved.

local special   = import("app.core.physics.static_character").create("door")

function special:onCreate(args)
    self.lock_time_ = 1.0
end

function special:onAfterAnimate(args)
    self.start_position_ = args.anchored_position_
    self:setPosition(self.start_position_)

    self.triggered_ = false
end

function special:animate(cname)

    local lock   =  { name = "lock", animation = { name = cname .. "_" .. "lock", forever = false, delay = self.lock_time_ / 8} }

    self.sprite_:load_action(lock, false)
    self.sprite_:set_animation(cname .. "_" .. "lock")
    self.sprite_:set_image_index(4)

end

function special:reopen()
    self.kinematic_body_.current_shape_:setTag(cc.tags.door)
    self.triggered_ = false
end


function special:lock(callback)

    local delay = cc.DelayTime:create(self.lock_time_ * 0.5)

    local lock = cc.CallFunc:create(function()
        cc.audio.play_sfx("sounds/sfx_door.mp3", false)
        self.sprite_:run_action("lock")
    end)

    local post_lock = cc.CallFunc:create(function()
        callback()
    end)

    local sequence = cc.Sequence:create(lock, delay, post_lock, nil)

    self:stopAllActions()
    self:runAction(sequence)

    self.kinematic_body_.current_shape_:setTag(cc.tags.block)
end

function special:unlock(callback)
    local delay = cc.DelayTime:create(self.lock_time_ * 0.5)

    local lock = cc.CallFunc:create(function()
        cc.audio.play_sfx("sounds/sfx_door.mp3", false)
        self.sprite_:run_action("lock")
        self.sprite_:reverse_action()
    end)

    local post_lock = cc.CallFunc:create(function()
        callback()
    end)

    local sequence = cc.Sequence:create(lock, delay, post_lock, nil)

    self:stopAllActions()
    self:runAction(sequence)

    self.triggered_ = true
end

return special




