/**
 * Client-side Load Balancer for multiple ESP32 endpoints
 * Handles round-robin distribution and retry logic
 */

const ENDPOINTS = [
  'http://esp32-fan.local',
  'http://192.168.4.1',
  'http://172.18.1.109',
];

let currentEndpointIndex = 0;
const endpointStats = new Map<string, { failures: number; lastError: string }>();

// Initialize stats for all endpoints
ENDPOINTS.forEach(ep => {
  endpointStats.set(ep, { failures: 0, lastError: '' });
});

/**
 * Get next endpoint in round-robin fashion
 */
const getNextEndpoint = (): string => {
  const endpoint = ENDPOINTS[currentEndpointIndex % ENDPOINTS.length];
  currentEndpointIndex++;
  console.log(`[LB] Selected endpoint: ${endpoint}`);
  return endpoint;
};

/**
 * Reset failure count after successful request
 */
const recordSuccess = (endpoint: string) => {
  const stats = endpointStats.get(endpoint);
  if (stats) {
    stats.failures = 0;
    stats.lastError = '';
  }
};

/**
 * Record failure for an endpoint
 */
const recordFailure = (endpoint: string, error: string) => {
  const stats = endpointStats.get(endpoint);
  if (stats) {
    stats.failures++;
    stats.lastError = error;
    console.warn(`[LB] Endpoint failure (${stats.failures}): ${endpoint} - ${error}`);
  }
};

/**
 * Main fetch wrapper with load balancing and retry
 */
export const fetchWithLoadBalancing = async (
  path: string,
  options: RequestInit = {},
  maxRetries: number = 3
): Promise<Response> => {
  let lastError: Error | null = null;

  for (let attempt = 0; attempt < maxRetries; attempt++) {
    const endpoint = getNextEndpoint();
    const url = `${endpoint}${path}`;

    try {
      console.log(`[LB] Attempt ${attempt + 1}/${maxRetries} - Fetching: ${url}`);
      const response = await fetch(url, {
        ...options,
        signal: AbortSignal.timeout(5000), // 5s timeout
      });

      if (response.ok) {
        recordSuccess(endpoint);
        return response;
      }

      // Non-OK response
      const error = `HTTP ${response.status}`;
      recordFailure(endpoint, error);
      lastError = new Error(error);

      if (attempt < maxRetries - 1) {
        console.log(`[LB] Retrying after non-OK response...`);
        await new Promise(resolve => setTimeout(resolve, 200)); // Small delay before retry
      }
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : String(error);
      recordFailure(endpoint, errorMsg);
      lastError = error instanceof Error ? error : new Error(errorMsg);

      if (attempt < maxRetries - 1) {
        console.log(`[LB] Request failed, retrying with next endpoint...`);
        await new Promise(resolve => setTimeout(resolve, 100)); // Small delay before retry
      }
    }
  }

  // All retries exhausted
  throw new Error(
    `Failed to reach any endpoint after ${maxRetries} attempts. Last error: ${lastError?.message}`
  );
};

/**
 * Get current endpoint statistics (for debugging)
 */
export const getEndpointStats = () => {
  return Array.from(endpointStats.entries()).map(([endpoint, stats]) => ({
    endpoint,
    ...stats,
    healthy: stats.failures < 2,
  }));
};

/**
 * Reset all statistics (useful for testing/recovery)
 */
export const resetStats = () => {
  currentEndpointIndex = 0;
  ENDPOINTS.forEach(ep => {
    const stats = endpointStats.get(ep);
    if (stats) {
      stats.failures = 0;
      stats.lastError = '';
    }
  });
};

/**
 * Get list of endpoints
 */
export const getEndpoints = () => ENDPOINTS;
