const queueBanner = document.getElementById('queue-banner');
const form = document.getElementById('search-form');
const input = document.getElementById('search-input');
const resultsArea = document.getElementById('results-area');
const snippetsArea = document.getElementById('snippets-area');

async function fetchWithQueueHandling(url, options) {
    while (true) {
        try {
            const response = await fetch(url, options);
            const data = await response.json();

            if (data.status === 'busy') {
                if (queueBanner) {
                    queueBanner.style.display = 'block';
                    queueBanner.textContent = 'You are in the queue. Please wait for a free slot.';
                }

                if (resultsArea) {
                    resultsArea.innerHTML = '<p class="status-message error">Waiting in queue...</p>';
                }

                await new Promise(r => setTimeout(r, 2000));
                continue;
            } else {
                if (queueBanner) {
                    queueBanner.style.display = 'none';
                }
                return {ok: response.ok, data};
            }
        } catch (error) {
            throw error;
        }
    }
}
form.addEventListener('submit', async (event) => {
    event.preventDefault();

    const query = input.value.trim();
    if (!query) {
        return;
    }

    resultsArea.innerHTML = '<p class="status-message info">Searching...</p>';
    snippetsArea.innerHTML = '';

    try {
        const {ok, data} = await fetchWithQueueHandling('http://localhost:3000/search', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({query})
        });

        if (!ok) {
            resultsArea.innerHTML = `<p class="status-message error">${data.error || 'Error'}</p>`;
            return;
        }

        if (!data.results || data.results.length === 0) {
            resultsArea.innerHTML = '<p class="status-message info">No results found.</p>';
            return;
        }

        const list = document.createElement('ul');
        list.classList.add('results-list');

        data.results.forEach((r) => {
            const li = document.createElement('li');
            li.classList.add('result-item');
            li.innerHTML = `<strong>[${r.index}]</strong> ${r.path} ` + `<span class="matches-count">(matches: ${r.matches})</span>`;
            li.dataset.index = r.index;

            li.addEventListener('click', () => {
                list.querySelectorAll('.result-item').forEach(item => item.classList.remove('selected'));
                li.classList.add('selected');

                showSnippets(li.dataset.index);
            });

            list.appendChild(li);
        });

        resultsArea.innerHTML = '';
        resultsArea.appendChild(list);

        snippetsArea.innerHTML = '<p class="status-message info">Click on a result to view snippets.</p>';
    } catch (error) {
        console.error(error);
        resultsArea.innerHTML = '<p class="status-message error">Network error.</p>';
        if (queueBanner) {
            queueBanner.style.display = 'none';
        }
    }
});

async function showSnippets(index) {
    snippetsArea.innerHTML = '<p class="status-message info">Loading snippets from C++ server...</p>';

    try {
        const response = await fetch('http://localhost:3000/snippet', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ index: Number(index) })
        });

        const data = await response.json();

        if (!response.ok) {
            snippetsArea.innerHTML = `<p class="status-message error">${data.error || 'Failed to load snippets'}</p>`;
            return;
        }

        if (!data.snippets || data.snippets.length === 0) {
            snippetsArea.innerHTML = '<p class="status-message info">No snippets found.</p>';
            return;
        }

        snippetsArea.innerHTML = '';

        const header = document.createElement('h3');
        header.textContent = 'Snippets';
        header.classList.add('snippets-header');
        snippetsArea.appendChild(header);

        data.snippets.forEach((snippetText, idx) => {
            const block = document.createElement('div');
            block.classList.add('snippet-item');

            const numberDiv = document.createElement('div');
            numberDiv.classList.add('snippet-number');
            numberDiv.textContent = `#${idx + 1}`;

            const contentDiv = document.createElement('div');
            contentDiv.classList.add('snippet-content');
            contentDiv.textContent = snippetText;

            block.appendChild(numberDiv);
            block.appendChild(contentDiv);

            snippetsArea.appendChild(block);
        });
    } catch (error) {
        console.error(error);
        snippetsArea.innerHTML = '<p class="status-message error">Network error while loading snippets.</p>';
    }
}