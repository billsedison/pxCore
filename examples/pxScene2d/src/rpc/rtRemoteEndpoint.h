#ifndef __RT_REMOTE_ENDPOINT_H__
#define __RT_REMOTE_ENDPOINT_H__

#include <string>
#include <stdint.h>

////// BASE //////

/* Abstract base class for endpoint addresses */
class rtRemoteIEndpoint
{
public:
	rtRemoteIEndpoint(std::string const& scheme);
	virtual ~rtRemoteIEndpoint();

	virtual std::string toUriString() = 0;

	inline std::string scheme() const
	  { return m_scheme; }

protected:
	std::string m_scheme;
};

////// LOCAL //////

/* stores address information for unix domain sockets, named pipes, files, shared memory, etc. */
class rtRemoteEndpointLocal : public virtual rtRemoteIEndpoint
{
public:
	rtRemoteEndpointLocal(std::string const& scheme, std::string const& path);
	
	virtual std::string toUriString() override;

	inline bool isSocket() const
    { return m_scheme.compare("tcp") == 0 || m_scheme.compare("udp") == 0; }

	inline std::string path() const
	  { return m_path; }
	
	inline bool operator==(rtRemoteEndpointLocal const& rhs) const
	  { return m_path.compare(rhs.path()) == 0
		      && m_scheme.compare(rhs.scheme()) == 0;
		}

protected:
	std::string m_path;
};

////// REMOTE //////

/* stores address information for remote sockets (tcp, udp, http, etc.) */
class rtRemoteEndpointRemote : public virtual rtRemoteIEndpoint
{
public:
  rtRemoteEndpointRemote(std::string const& scheme, std::string const& host, int port);
	
	virtual std::string toUriString() override;

	inline std::string host() const
	  { return m_host; }

	inline uint16_t port() const
	  { return m_port; }
	
	inline bool operator==(rtRemoteEndpointRemote const& rhs) const
	  { return m_host.compare(rhs.host()) == 0
		      && m_port == rhs.port()
		      && m_scheme.compare(rhs.scheme()) == 0;
		}

protected:
	std::string m_host;
	uint16_t    m_port;
};

////// REMOTE + PATH //////

/* Remote endpoints that include path information
 *
 * Currently not used.  If use case arises, must be integrated throughout the code.
 */
class rtRemoteEndpointDistributed : public rtRemoteEndpointRemote, public rtRemoteEndpointLocal
{
public:
	rtRemoteEndpointDistributed(std::string const& scheme, std::string const& host, int port, std::string const& path);
	virtual std::string toUriString() override;
};

#endif