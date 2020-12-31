-- Copyright 2014-2015 Greentwip. All Rights Reserved.

local weapon        = import("app.objects.weapons.base.weapon")
local directional_star_bullet = class("directional_star_bullet", weapon)


function directional_star_bullet:onCreate()
    self.super:onCreate()
    self.power_    = 6
end

function directional_star_bullet:animate(cname)

    local action = {name = "walk",   animation = {name = cname .. "_" .. "walk",  forever = true, delay = 0.10} }

    self.sprite_:load_action(action, false)
    self.sprite_:run_action("walk")

end

function directional_star_bullet:step(dt)
    self.current_speed_ = self.speed_
    return self
end


return directional_star_bullet