#include "resource_base.hpp"

ResourceBase::ResourceBase(const std::string & node_name, const std::string & publish_topic, const std::string & subscribe_topic, 
                           const std::string & authentication_topic)
: Node(node_name)
{
    node_name_ = node_name;

    T_ = std::make_shared<TalkerNode>(node_name + "_talker", publish_topic);
    L_ = std::make_shared<ListenerNode>(node_name + "_listener", subscribe_topic);

    if(authentication_topic.size() > 0)
    {
        T_auth_ = std::make_shared<TalkerNode>(node_name_ + "_auth" + "_talker", authentication_topic);
        L_auth_ = std::make_shared<ListenerNode>(node_name_ + "_auth" + "_listener", authentication_topic);
    }

    M_received_fresh_ = false;
    MS_received_fresh_ = false;
}

void
ResourceBase::run(void)
{
    publish_macaroon();
    exec_.spin_node_some(L_);  // allow the ListenerNode callback to execute
    receive_macaroon();
}

// Initialise a discharge Macaroon.  Caveats added after.
// This function could be used by an owner, intermediary (e.g., third party) or user
void
ResourceBase::initialise_discharge_macaroon(const std::string location, const std::string key, const std::string identifier)
{
    D_.initialise(location, key, identifier);
}

// Send a request to a resource owner to initiate TOFU
// TODO:  Possibly move the 'resource' to a private variable?  For this PoC, we're only dealing with one resource.
void
ResourceBase::request_resource_access(const std::string resource)
{
    // TODO: this should generate a key and an id.  possibly the id should just be the resource??
    TOFU_key_ = "another_bad_key:";
    TOFU_location_ = "https://www.unused_third_party.com/";
    TOFU_identifier_ = "another_bad_key_id";
    TOFU_resource_ = resource;

    // publish the request
    (*T_auth_).publish_resource_access_request(TOFU_key_, TOFU_location_, TOFU_identifier_, TOFU_resource_);
}

// Add a first party caveat to the "owned" Macaroon by calling the Base class
void
ResourceBase::add_first_party_caveat(const std::string first_party_caveat)
{
    first_party_caveats_.push_back(first_party_caveat);
}

// Add a third party caveat to the "owned" Macaroon by calling the Base class
void
ResourceBase::add_third_party_caveat(const std::string location, const std::string key, const std::string identifier)
{
    ThirdPartyCaveat tpc = {location, key, identifier};
    third_party_caveats_.push_back(tpc);
}

// Adds first and third party caveats and publishes a serialised Macaroon
void
ResourceBase::publish_macaroon(void)
{
    if(M_.initialised())
    {
        // Derive a new Macaroon from M_ and add caveats
        Macaroon M_send = M_;
        for(std::string fpc : first_party_caveats_)
        {
            M_send.add_first_party_caveat(fpc);
        }
        for(ThirdPartyCaveat tpc : third_party_caveats_)
        {
            M_send.add_third_party_caveat(tpc.location, tpc.key, tpc.identifier);
        }

        // publish the serialised Macaroon with caveats
        (*T_).publish_message(M_send.serialise());

        std::vector<std::string> macaroons;
        macaroons.push_back(M_send.serialise());
        (*T_).publish_macaroons_message(macaroons);
    }
}

void
ResourceBase::receive_macaroon(void)
{
    std::string msg_received = (*L_).get_message();
    std::vector<std::string> macaroons_msg_received = (*L_).get_macaroons_message();
    if (msg_received.size() > 0)
    {
        M_received_.deserialise(msg_received);

        if(M_received_.initialised())
        {
            M_received_fresh_ = true;
        }
    }
    if (macaroons_msg_received.size() > 0)
    {
        for(std::string macaroon : macaroons_msg_received)
        {
            MS_received_.push_back(Macaroon(macaroon));
        }

        MS_received_fresh_ = true;

        for(Macaroon M : MS_received_)
        {
            if(!M.initialised())
            {
                MS_received_fresh_ = false;
            }
        }
    }
}