#include "stage1.hpp"

// Define a conversion from the SDL_FRect to a boost::json::value
void tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    SDL_FRect const& region
    ) {

    jv = {
        {"x", region.x},
        {"y", region.y},
        {"w", region.w},
        {"h", region.h}
    };
}

Stage1::Stage1(SDL_Renderer* ctx, std::shared_ptr<TextureFactory> factory)
    : Stage{ctx, factory} {

    // Player
    player_ = std::make_shared<Player>(ctx, factory);

    player_->set_region(
        (WINDOW_WIDTH / 2) + (PLAYER_WIDTH / 2),
        WINDOW_HEIGHT - (PLAYER_HEIGHT * 2),
        PLAYER_WIDTH,
        PLAYER_HEIGHT
    );

    player_->set_base_speed(PLAYER_BASE_SPEED);
    player_->set_bullet_dx(PLAYER_BULLET_DX);
    player_->set_bullet_dy(PLAYER_BULLET_DY);
    player_->set_bullet_rate(PLAYER_BULLET_RATE);
    player_->set_bullet_health(PLAYER_BULLET_HEALTH);
    player_->set_stage1(this);
    player_->load_texture_from_path(PLAYER_TEXTURE);

    // Meiling
    meiling_ = std::make_shared<Meiling>(ctx, factory);
    meiling_->set_region(WINDOW_WIDTH, 0, ENEMY_WIDTH, ENEMY_HEIGHT);
    meiling_->load_texture_from_path(ENEMY_TEXTURE);
    meiling_->set_bullet_health(ENEMY_BULLET_HEALTH);
}

Stage1::~Stage1() {
    if (t_ != nullptr)
        if (t_->joinable())
            t_->join();
}

StageTag Stage1::start() {
    auto ctx = get_renderer();
    auto factory = get_factory();

    // Spawn a thread that observe the current event
    auto k = KeyEventListener();
    auto rx = k.activate();

    // Glue above thread to the player object
    player_->subscribe(std::move(rx));

    // // Wait for the client to connect
    auto sock = Socket();
    sock.accept_client();

    auto prev_ticks = SDL_GetTicks();

    spawn_worker(); // Invoke the worker thread

    for (;;) {
        if (player_->quit_.load()) { // ask player to exit the loop
            quit_.store(true);
            break;
        }

        SDL_RenderClear(ctx);

        player_->lock();
        auto& m_bullets = meiling_->get_bullets();

        for (auto& b : m_bullets) {
            if (player_->collided_with(b)) {
                player_->set_x((WINDOW_WIDTH / 2) + (PLAYER_WIDTH / 2));
                player_->set_y(WINDOW_HEIGHT - (PLAYER_HEIGHT * 2));
            }
        }

        player_->unlock();

        // Update player
        player_->lock();
        player_->update();
        player_->unlock();

        // Update enemy
        meiling_->update();

        // Send a frame data via socket

        player_->lock();
        auto p_region = player_->get_region();
        player_->unlock();

        auto e_region = meiling_->get_region();
        auto& e_bullets = meiling_->get_bullets();

        std::vector<SDL_FRect> e_bullets_region;
        for (auto& bullet : e_bullets) {
            auto region = bullet->get_region();
            e_bullets_region.push_back(region);
        }

        boost::json::object frame;

        auto p_region_json = boost::json::value_from(p_region);
        auto e_region_json = boost::json::value_from(e_region);
        auto e_bullets_region_json = boost::json::value_from(e_bullets_region);

        frame["player"] = p_region_json;
        frame["enemy"] = e_region_json;
        frame["enemy_bullets"] = e_bullets_region_json;

        auto data_str = boost::json::serialize(frame);
        sock.send_data(data_str);

        // Calculate the delay manually
        auto current_ticks = SDL_GetTicks();
        auto elapsed = current_ticks - prev_ticks;

        if (elapsed < WINDOW_FRAME_LENGTH)
            SDL_Delay(WINDOW_FRAME_LENGTH - elapsed);

        prev_ticks = current_ticks;

        SDL_RenderPresent(ctx);
    }

    /*
    Don't forget to unsubscribe thread, player object's destructor will
    wait the join of the subscribed thread so this program will freeze.
     */
    player_->unsubscribe();

    return StageTag::EXIT;
}

void Stage1::spawn_worker() {
    if (t_ != nullptr)
        if (t_->joinable())
            t_->detach();

    t_ = std::make_unique<std::thread>();

    std::thread thread([this]{
        debug_log("DEBUG: Stage 1 worker thread has been spawned");

        // auto ctx = get_renderer();
        // auto factory = get_factory();

        /*
        Do what you whatever you want here
         */

        debug_log("DEBUG: Stage 1 worker thread has been successfully terminated");
    });

    if (t_ != nullptr)
    {
        thread.swap(*t_);
    }
    else
    {
        debug_log("FATAL: Failed to spawn a stage 1 worker thread");
    }
}
